/*
 driver/usbpd/usbpd_cc.c - USB PD(Power Delivery) device driver
 *
 * Copyright (C) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/usb_notify.h>
#include <linux/version.h>

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/slsi/common/usbpd_ext.h>
#endif
#include <linux/usb/typec/slsi/common/usbpd.h>

#if IS_ENABLED(CONFIG_PDIC_S2MU004)
#include <linux/usb/typec/slsi/s2mu004/usbpd-s2mu004.h>
#elif IS_ENABLED(CONFIG_PDIC_S2MU106)
#include <linux/usb/typec/slsi/s2mu106/usbpd-s2mu106.h>
#elif IS_ENABLED(CONFIG_PDIC_S2MU205)
#include <linux/usb/typec/slsi/s2mu205/usbpd-s2mu205.h>
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
#include <linux/usb/typec/slsi/s2mu107/usbpd-s2mu107.h>
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
#include <linux/usb/typec/slsi/s2mf301/usbpd-s2mf301.h>
#endif

#if IS_ENABLED(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int typec_dr_set(struct typec_port *_port, enum typec_data_role role);
static int typec_pr_set(struct typec_port *_port, enum typec_role role);
int typec_port_type_set(struct typec_port *_port, enum typec_port_type port_type);
const struct typec_operations slsi_typec_ops = {
	.try_role = NULL,
	.dr_set		= typec_dr_set,
	.pr_set		= typec_pr_set,
	.port_type_set	=	typec_port_type_set,
};
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
static void pdic_event_notifier(struct work_struct *data)
{
	struct pdic_state_work *event_work =
		container_of(data, struct pdic_state_work, pdic_work);
	PD_NOTI_TYPEDEF pdic_noti;

	switch (event_work->dest) {
	case PDIC_NOTIFY_DEV_USB:
		pr_info("usb:%s, dest=%s, id=%s, attach=%s, drp=%s\n", __func__,
				pdic_event_dest_string(event_work->dest),
				pdic_event_id_string(event_work->id),
				event_work->attach ? "Attached" : "Detached",
				pdic_usbstatus_string(event_work->event));
		break;
	default:
		pr_info("usb:%s, dest=%s, id=%s, attach=%d, event=%d\n", __func__,
			pdic_event_dest_string(event_work->dest),
			pdic_event_id_string(event_work->id),
			event_work->attach,
			event_work->event);
		break;
	}

	pdic_noti.src = PDIC_NOTIFY_DEV_PDIC;
	pdic_noti.dest = event_work->dest;
	pdic_noti.id = event_work->id;
	pdic_noti.sub1 = event_work->attach;
	pdic_noti.sub2 = event_work->event;
	pdic_noti.sub3 = event_work->sub;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	pdic_noti.pd = &g_pd_data->pd_noti;
#endif
	pdic_notifier_notify((PD_NOTI_TYPEDEF *)&pdic_noti, NULL, 0);

	kfree(event_work);
}

void pdic_event_work(void *data, int dest, int id, int attach, int event, int sub)
{
	struct usbpd_data *pd_data = data;
	struct pdic_state_work *event_work;
#if IS_ENABLED(CONFIG_TYPEC)
	struct typec_partner_desc desc = {0};
	enum typec_pwr_opmode mode;
#endif

	event_work = kmalloc(sizeof(struct pdic_state_work), GFP_ATOMIC);
	if (!event_work) {
		pr_info("usb: %s,event_work is null", __func__);
		return;
	}
	pr_info("usb: %s,event_work\n", __func__);
	INIT_WORK(&event_work->pdic_work, pdic_event_notifier);

	event_work->dest = dest;
	event_work->id = id;
	event_work->attach = attach;
	event_work->event = event;
	event_work->sub = sub;

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
	if (id == PDIC_NOTIFY_ID_USB) {
		pr_info("khos usb: %s, dest=%d, event=%d, sub=%d, usbpd_data->data_role_dual=%d, usbpd_data->try_state_change=%d\n",
			__func__, dest, event, sub, usbpd_data->data_role_dual, usbpd_data->try_state_change);

		usbpd_data->data_role_dual = event;

		if (usbpd_data->dual_role != NULL)
			dual_role_instance_changed(usbpd_data->dual_role);

		if (usbpd_data->try_state_change &&
			(usbpd_data->data_role_dual != USB_STATUS_NOTIFY_DETACH)) {
			/* Role change try and new mode detected */
			pr_info("usb: %s, reverse_completion\n", __func__);
			complete(&usbpd_data->reverse_completion);
		}
	}
	else if (id == PDIC_NOTIFY_ID_ROLE_SWAP ) {
		if (usbpd_data->dual_role != NULL)
			dual_role_instance_changed(usbpd_data->dual_role);
	}
#elif IS_ENABLED(CONFIG_TYPEC)
	if (id == PDIC_NOTIFY_ID_USB) {
		if (pd_data->typec_try_state_change &&
			(event != USB_STATUS_NOTIFY_DETACH)) {
			// Role change try and new mode detected
			pr_info("usb: %s, role_reverse_completion\n", __func__);
			complete(&pd_data->role_reverse_completion);
		}
		if (event == USB_STATUS_NOTIFY_ATTACH_UFP) {
			mode = typec_get_pd_support(pd_data);
			typec_set_pwr_opmode(pd_data->port, mode);
			desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
			desc.accessory = TYPEC_ACCESSORY_NONE; /* XXX: handle accessories */
			desc.identity = NULL;
			pd_data->typec_data_role = TYPEC_DEVICE;
			typec_set_data_role(pd_data->port, TYPEC_DEVICE);
			pd_data->partner = typec_register_partner(pd_data->port, &desc);
		} else if (event == USB_STATUS_NOTIFY_ATTACH_DFP) {
			mode = typec_get_pd_support(pd_data);
			typec_set_pwr_opmode(pd_data->port, mode);
			desc.usb_pd = mode == TYPEC_PWR_MODE_PD;
			desc.accessory = TYPEC_ACCESSORY_NONE; /* XXX: handle accessories */
			desc.identity = NULL;
			pd_data->typec_data_role = TYPEC_HOST;
			typec_set_data_role(pd_data->port, TYPEC_HOST);
			pd_data->partner = typec_register_partner(pd_data->port, &desc);
		} else {
			if (!IS_ERR(pd_data->partner))
				typec_unregister_partner(pd_data->partner);
			pd_data->partner = NULL;
		}
	}
#endif

	if (queue_work(pd_data->pdic_wq, &event_work->pdic_work) == 0) {
		pr_info("usb: %s, event_work is dropped\n", __func__);
		kfree(event_work);
	}
}
EXPORT_SYMBOL(pdic_event_work);
#endif

#if defined(CONFIG_DUAL_ROLE_USB_INTF)
static enum dual_role_property fusb_drp_properties[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	DUAL_ROLE_PROP_VCONN_SUPPLY,
};

void role_swap_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
#if IS_ENABLED(CONFIG_PDIC_S2MU004)
	struct s2mu004_usbpd_data *usbpd_data =
		container_of(delay_work, struct s2mu004_usbpd_data, role_swap_work);
#elif IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *usbpd_data =
		container_of(delay_work, struct s2mu106_usbpd_data, role_swap_work);
#elif IS_ENABLED(CONFIG_PDIC_S2MU205)
	struct s2mu205_usbpd_data *usbpd_data =
		container_of(delay_work, struct s2mu205_usbpd_data, role_swap_work);
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *usbpd_data =
		container_of(delay_work, struct s2mu107_usbpd_data, role_swap_work);
#endif
	int mode = 0;

	pr_info("%s: pdic_set_dual_role check again.\n", __func__);
	usbpd_data->try_state_change = 0;

	if (usbpd_data->detach_valid) { /* modify here using pd_state */
		pr_err("%s: pdic_set_dual_role reverse failed, set mode to DRP\n", __func__);
		PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 0);
		/* exit from Disabled state and set mode to DRP */
		mode =  TYPE_C_ATTACH_DRP;

		PDIC_OPS_PARAM_FUNC(rprd_mode_change, usbpd_data, mode);
		PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 1);
	}
}

static int pdic_set_dual_role(struct dual_role_phy_instance *dual_role,
				   enum dual_role_property prop,
				   const unsigned int *val)
{
#if IS_ENABLED(CONFIG_PDIC_S2MU004)
	struct s2mu004_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#elif IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#elif IS_ENABLED(CONFIG_PDIC_S2MU205)
	struct s2mu205_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#endif
	struct i2c_client *i2c;

	USB_STATUS attached_state;
	int mode;
	int timeout = 0;
	int ret = 0;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null \n", __func__);
		return -EINVAL;
	}

	i2c = usbpd_data->i2c;

	/* Get Current Role */
	attached_state = usbpd_data->data_role_dual;
	pr_info("%s : request prop = %d , attached_state = %d\n", __func__, prop, attached_state);

	if (attached_state != USB_STATUS_NOTIFY_ATTACH_DFP
	    && attached_state != USB_STATUS_NOTIFY_ATTACH_UFP) {
		pr_err("%s : current mode : %d - just return \n", __func__, attached_state);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP
	    && *val == DUAL_ROLE_PROP_MODE_DFP) {
		pr_err("%s : current mode : %d - request mode : %d just return \n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP
	    && *val == DUAL_ROLE_PROP_MODE_UFP) {
		pr_err("%s : current mode : %d - request mode : %d just return \n",
			__func__, attached_state, *val);
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		/* Current mode DFP and Source  */
		pr_info("%s: try reversing, from Source to Sink\n", __func__);
		usbpd_manager_vbus_turn_on_ctrl(pd_data, 0);
#if defined(CONFIG_MUIC_SUPPORT_PDIC_OTG_CTRL)
		muic_disable_otg_detect();
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
		/* muic */
		pdic_event_work(usbpd_data,
			PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH, 0/*attach*/, 0/*rprd*/, 0);
#endif
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_UFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_UFP;
		PDIC_OPS_PARAM_FUNC(rprd_mode_change, usbpd_data, TYPE_C_ATTACH_UFP);
	} else {
		/* Current mode UFP and Sink  */
		pr_info("%s: try reversing, from Sink to Source\n", __func__);
		/* exit from Disabled state and set mode to UFP */
		mode =  TYPE_C_ATTACH_DFP;
		usbpd_data->try_state_change = TYPE_C_ATTACH_DFP;
		PDIC_OPS_PARAM_FUNC(rprd_mode_change, usbpd_data, TYPE_C_ATTACH_DFP);
	}

	reinit_completion(&usbpd_data->reverse_completion);
	timeout =
	    wait_for_completion_timeout(&usbpd_data->reverse_completion,
					msecs_to_jiffies
					(DUAL_ROLE_SET_MODE_WAIT_MS));

	if (!timeout) {
		usbpd_data->try_state_change = 0;
		pr_err("%s: reverse failed, set mode to DRP\n", __func__);
		PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 0);
		/* exit from Disabled state and set mode to DRP */
		mode =  TYPE_C_ATTACH_DRP;
		PDIC_OPS_PARAM_FUNC(rprd_mode_change, usbpd_data, mode);
		PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 2);
		ret = -EIO;
	} else {
		pr_err("%s: reverse success, one more check\n", __func__);
		schedule_delayed_work(&usbpd_data->role_swap_work, msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
	}

	dev_info(&i2c->dev, "%s -> data role : %d\n", __func__, *val);
	return ret;
}

/* Decides whether userspace can change a specific property */
int dual_role_is_writeable(struct dual_role_phy_instance *drp,
				  enum dual_role_property prop)
{
	if (prop == DUAL_ROLE_PROP_MODE)
		return 1;
	else
		return 0;
}

/* Callback for "cat /sys/class/dual_role_usb/otg_default/<property>" */
int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
				    enum dual_role_property prop,
				    unsigned int *val)
{
#if IS_ENABLED(CONFIG_PDIC_S2MU004)
	struct s2mu004_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#elif IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#elif IS_ENABLED(CONFIG_PDIC_S2MU205)
	struct s2mu205_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *usbpd_data = dual_role_get_drvdata(dual_role);
#endif

	USB_STATUS attached_state;
	int power_role_dual;

	if (!usbpd_data) {
		pr_err("%s : usbpd_data is null : request prop = %d \n", __func__, prop);
		return -EINVAL;
	}
	attached_state = usbpd_data->data_role_dual;
	power_role_dual = usbpd_data->power_role_dual;

	pr_info("%s : request prop = %d , attached_state = %d, power_role_dual = %d\n",
		__func__, prop, attached_state, power_role_dual);

	if (prop == DUAL_ROLE_PROP_VCONN_SUPPLY) {
		if (usbpd_data->vconn_en)
			*val = DUAL_ROLE_PROP_VCONN_SUPPLY_YES;
		else
			*val = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
		return 0;
	}

	if (attached_state == USB_STATUS_NOTIFY_ATTACH_DFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_DFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_HOST;
		else
			return -EINVAL;
	} else if (attached_state == USB_STATUS_NOTIFY_ATTACH_UFP) {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_UFP;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = power_role_dual;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_DEVICE;
		else
			return -EINVAL;
	} else {
		if (prop == DUAL_ROLE_PROP_MODE)
			*val = DUAL_ROLE_PROP_MODE_NONE;
		else if (prop == DUAL_ROLE_PROP_PR)
			*val = DUAL_ROLE_PROP_PR_NONE;
		else if (prop == DUAL_ROLE_PROP_DR)
			*val = DUAL_ROLE_PROP_DR_NONE;
		else
			return -EINVAL;
	}

	return 0;
}

/* Callback for "echo <value> >
 *                      /sys/class/dual_role_usb/<name>/<property>"
 * Block until the entire final state is reached.
 * Blocking is one of the better ways to signal when the operation
 * is done.
 * This function tries to switch to Attached.SRC or Attached.SNK
 * by forcing the mode into SRC or SNK.
 * On failure, we fall back to Try.SNK state machine.
 */
int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
			      enum dual_role_property prop,
			      const unsigned int *val)
{
	pr_info("%s : request prop = %d , *val = %d \n", __func__, prop, *val);
	if (prop == DUAL_ROLE_PROP_MODE)
		return pdic_set_dual_role(dual_role, prop, val);
	else
		return -EINVAL;
}

int dual_role_init(void *_data)
{
#if IS_ENABLED(CONFIG_PDIC_S2MU004)
	struct s2mu004_usbpd_data *pdic_data = _data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU106)
	struct s2mu106_usbpd_data *pdic_data = _data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU205)
	struct s2mu205_usbpd_data *pdic_data = _data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
	struct s2mu107_usbpd_data *pdic_data = _data;
#endif
	struct dual_role_phy_desc *desc;
	struct dual_role_phy_instance *dual_role;

	desc = devm_kzalloc(pdic_data->dev,
			 sizeof(struct dual_role_phy_desc), GFP_KERNEL);
	if (!desc) {
		pr_err("unable to allocate dual role descriptor\n");
		return -1;
	}

	desc->name = "otg_default";
	desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
	desc->get_property = dual_role_get_local_prop;
	desc->set_property = dual_role_set_prop;
	desc->properties = fusb_drp_properties;
	desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
	desc->property_is_writeable = dual_role_is_writeable;
	dual_role =
		devm_dual_role_instance_register(pdic_data->dev, desc);
	dual_role->drv_data = pdic_data;
	pdic_data->dual_role = dual_role;
	pdic_data->desc = desc;
	init_completion(&pdic_data->reverse_completion);
	INIT_DELAYED_WORK(&pdic_data->role_swap_work, role_swap_check);

	return 0;
}
#elif IS_ENABLED(CONFIG_TYPEC)
void typec_role_swap_check(struct work_struct *wk)
{
	struct delayed_work *delay_work =
		container_of(wk, struct delayed_work, work);
	struct usbpd_data *pd_data =
		container_of(delay_work, struct usbpd_data, typec_role_swap_work);

	pr_info("%s: pdic_set_dual_role check again.\n", __func__);
	pd_data->typec_try_state_change = 0;

	if (PDIC_OPS_FUNC(get_detach_valid, pd_data)) { /* modify here using pd_state */
		pr_err("%s: pdic_set_dual_role reverse failed, set mode to DRP\n", __func__);
		PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 0);
		/* exit from Disabled state and set mode to DRP */
		PDIC_OPS_PARAM_FUNC(rprd_mode_change, pd_data->phy_driver_data, TYPE_C_ATTACH_DRP);
		PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 1);
	}
}

#if IS_ENABLED(CONFIG_SUPPORT_USB_TYPEC_OPS)
int typec_port_type_set(struct typec_port *_port, enum typec_port_type port_type)
{
	struct usbpd_data *pd_data = typec_get_drvdata(_port);
#else
int typec_port_type_set(const struct typec_capability *cap, enum typec_port_type port_type)
{
	struct usbpd_data *pd_data = container_of(cap, struct usbpd_data, typec_cap);
#endif

	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return -EINVAL;
	} else {
#if IS_ENABLED(CONFIG_PDIC_S2MU004)
		struct s2mu004_usbpd_data *usbpd_data = (struct s2mu004_usbpd_data *)pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU106)
		struct s2mu106_usbpd_data *usbpd_data = (struct s2mu106_usbpd_data *)pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU205)
		struct s2mu205_usbpd_data *usbpd_data = (struct s2mu205_usbpd_data *)pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MU107)
		struct s2mu107_usbpd_data *usbpd_data = (struct s2mu107_usbpd_data *)pd_data->phy_driver_data;
#elif IS_ENABLED(CONFIG_PDIC_S2MF301)
		struct s2mf301_usbpd_data *usbpd_data = (struct s2mf301_usbpd_data *)pd_data->phy_driver_data;
#endif
		int timeout = 0;

		pr_info("%s : typec_power_role=%d, typec_data_role=%d, port_type=%d\n",
			__func__, pd_data->typec_power_role, pd_data->typec_data_role, port_type);

		switch (port_type) {
		case TYPEC_PORT_DFP:
			pr_info("%s : try reversing, from UFP(Sink) to DFP(Source)\n", __func__);
			pd_data->typec_try_state_change = TYPE_C_ATTACH_DFP;
			PDIC_OPS_PARAM_FUNC(rprd_mode_change, usbpd_data, TYPE_C_ATTACH_DFP);
			break;
		case TYPEC_PORT_UFP:
			pr_info("%s : try reversing, from DFP(Source) to UFP(Sink)\n", __func__);
			usbpd_manager_vbus_turn_on_ctrl(pd_data, 0);
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
			pdic_event_work(pd_data,
				PDIC_NOTIFY_DEV_MUIC, PDIC_NOTIFY_ID_ATTACH,
				0/*attach*/, 0/*rprd*/, 0);
#endif
			pd_data->typec_try_state_change = TYPE_C_ATTACH_UFP;
			PDIC_OPS_PARAM_FUNC(rprd_mode_change, usbpd_data, TYPE_C_ATTACH_UFP);
			break;
		case TYPEC_PORT_DRP:
			pr_info("%s : set to DRP (No action)\n", __func__);
			return 0;
		default :
			pr_info("%s : invalid typec_role\n", __func__);
			return -EINVAL;
		}

		if (pd_data->typec_try_state_change) {
			reinit_completion(&pd_data->role_reverse_completion);
			timeout =
			    wait_for_completion_timeout(&pd_data->role_reverse_completion,
							msecs_to_jiffies
							(DUAL_ROLE_SET_MODE_WAIT_MS));

			if (!timeout) {
				pr_err("%s: reverse failed, set mode to DRP\n", __func__);
				PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 0);
				/* exit from Disabled state and set mode to DRP */
				pd_data->typec_try_state_change = 0;
				PDIC_OPS_PARAM_FUNC(rprd_mode_change, usbpd_data, TYPE_C_ATTACH_DRP);
				PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 1);
				return -EIO;
			} else {
				pr_err("%s: reverse success, one more check\n", __func__);
				schedule_delayed_work(&pd_data->typec_role_swap_work, msecs_to_jiffies(DUAL_ROLE_SET_MODE_WAIT_MS));
			}
		}

		return 0;
	}
}

#if IS_ENABLED(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int typec_pr_set(struct typec_port *_port, enum typec_role power_role)
{
	struct usbpd_data *pd_data = typec_get_drvdata(_port);
#else
int typec_pr_set(const struct typec_capability *cap, enum typec_role power_role)
{
	struct usbpd_data *pd_data = container_of(cap, struct usbpd_data, typec_cap);
#endif
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_PR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */
	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return -EINVAL;
	}

	pr_info("%s : typec_power_role=%d, typec_data_role=%d, goto power role=%d\n",
									__func__, pd_data->typec_power_role,
									pd_data->typec_data_role, power_role);

	switch (power_role) {
	case TYPEC_SOURCE:
		pr_info("%s : try reversing, from Sink to Source\n", __func__);
		usbpd_manager_send_pr_swap(pd_data->dev);
		break;
	case TYPEC_SINK:
		pr_info("%s : try reversing, from Source to Sink\n", __func__);
		usbpd_manager_send_pr_swap(pd_data->dev);
		break;
	default :
		pr_info("%s : invalid power_role\n", __func__);
		return -EINVAL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SUPPORT_USB_TYPEC_OPS)
static int typec_dr_set(struct typec_port *_port, enum typec_data_role role)
{
	struct usbpd_data *pd_data = typec_get_drvdata(_port);
#else
static int typec_dr_set(const struct typec_capability *cap, enum typec_data_role role)
{
	struct usbpd_data *pd_data = container_of(cap, struct usbpd_data, typec_cap);
#endif
	int timeout = 0;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();

	if (o_notify)
		inc_hw_param(o_notify, USB_CCIC_DR_SWAP_COUNT);
#endif /* CONFIG_USB_HW_PARAM */
	if (!pd_data) {
		pr_err("%s : pd_data is null\n", __func__);
		return -EINVAL;
	}

	pr_info("%s : typec_power_role=%d, typec_data_role=%d, role=%d\n",
		__func__, pd_data->typec_power_role, pd_data->typec_data_role, role);

	if (role == TYPEC_DEVICE) {
		pr_info("%s, try reversing, from DFP to UFP\n", __func__);
		pd_data->typec_try_state_change = TYPE_C_DR_SWAP;
		usbpd_manager_send_dr_swap(pd_data->dev);
	} else if (role == TYPEC_HOST) {
		pr_info("%s, try reversing, from UFP to DFP\n", __func__);
		pd_data->typec_try_state_change = TYPE_C_DR_SWAP;
		usbpd_manager_send_dr_swap(pd_data->dev);
	} else {
		pr_info("invalid power role\n");
		return -EIO;
	}

	if (pd_data->typec_try_state_change) {
		reinit_completion(&pd_data->role_reverse_completion);
		timeout =
		    wait_for_completion_timeout(&pd_data->role_reverse_completion,
						msecs_to_jiffies
						(DUAL_ROLE_SET_MODE_WAIT_MS));

		if (!timeout) {
			pr_err("%s: reverse failed\n", __func__);
			PDIC_OPS_PARAM_FUNC(irq_control, pd_data, 0);
			/* exit from Disabled state and set mode to DRP */
			pd_data->typec_try_state_change = 0;
			return -EIO;
		} else
			pr_err("%s: reverse success\n", __func__);
	}

	return 0;
}

int typec_get_pd_support(void *_data)
{
	struct usbpd_data *pd_data = _data;
	struct policy_data *policy = &pd_data->policy;

	if (policy->pd_support)
		return TYPEC_PWR_MODE_PD;

	return TYPEC_PWR_MODE_USB;
}

int typec_init(struct usbpd_data *pd_data)
{
	pd_data->typec_power_role = TYPEC_SINK;
	pd_data->typec_data_role = TYPEC_DEVICE;
	pd_data->typec_cap.revision = USB_TYPEC_REV_1_2;
	pd_data->typec_cap.pd_revision = 0x300;
	pd_data->typec_cap.prefer_role = TYPEC_NO_PREFERRED_ROLE;
#if IS_ENABLED(CONFIG_SUPPORT_USB_TYPEC_OPS)
	pd_data->typec_cap.driver_data = pd_data;
	pd_data->typec_cap.ops = &slsi_typec_ops;
#else
	pd_data->typec_cap.pr_set = typec_pr_set;
	pd_data->typec_cap.dr_set = typec_dr_set;
 	pd_data->typec_cap.port_type_set = typec_port_type_set;
#endif
	pd_data->typec_cap.type = TYPEC_PORT_DRP;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	pd_data->typec_cap.data = TYPEC_PORT_DRD;
#endif
	pd_data->port = typec_register_port(pd_data->dev, &pd_data->typec_cap);
	if (IS_ERR(pd_data->port)) {
		pr_err("%s : unable to register typec_register_port\n", __func__);
		return -1;
	} else
		pr_err("%s : success typec_register_port port=%pK\n", __func__, pd_data->port);

	init_completion(&pd_data->role_reverse_completion);
	INIT_DELAYED_WORK(&pd_data->typec_role_swap_work, typec_role_swap_check);

	return 0;
}
EXPORT_SYMBOL(typec_init);
#endif
