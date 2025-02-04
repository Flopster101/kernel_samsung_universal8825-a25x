/*
*	USB PD Driver - Protocol Layer
*/

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/usb/typec/slsi/common/usbpd.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/pm_wakeup.h>
#include <linux/version.h>

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#if IS_ENABLED(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#else
#include <linux/battery/sec_pd.h>
#endif
struct usbpd_data *g_pd_data;
EXPORT_SYMBOL(g_pd_data);
#endif
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_sysfs.h>
#endif

#define MS_TO_NS(msec)		((msec) * 1000 * 1000)

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
enum pdic_sysfs_property usbpd_sysfs_properties[] = {
	PDIC_SYSFS_PROP_CHIP_NAME,
	PDIC_SYSFS_PROP_LPM_MODE,
	PDIC_SYSFS_PROP_STATE,
	PDIC_SYSFS_PROP_RID,
	PDIC_SYSFS_PROP_CTRL_OPTION,
	PDIC_SYSFS_PROP_FW_WATER,
	PDIC_SYSFS_PROP_ACC_DEVICE_VERSION,
	PDIC_SYSFS_PROP_USBPD_IDS,
	PDIC_SYSFS_PROP_USBPD_TYPE,
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
	PDIC_SYSFS_PROP_SET_WATER_THRESHOLD,
#endif
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	PDIC_SYSFS_PROP_USBPD_WATER_CHECK,
	PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE,
#endif
};
extern struct device *pdic_device;
#endif

void usbpd_timer_vdm_start(struct usbpd_data *pd_data)
{
	ktime_get_real_ts64(&pd_data->time_vdm);
}
EXPORT_SYMBOL(usbpd_timer_vdm_start);

long long usbpd_check_timer_vdm(struct usbpd_data *pd_data)
{
	uint32_t ms = 0;
	uint32_t sec = 0;
	struct timespec64 time;

	ktime_get_real_ts64(&time);

    sec = time.tv_sec - pd_data->time_vdm.tv_sec;
    ms = (uint32_t)((time.tv_nsec - pd_data->time_vdm.tv_nsec) / 1000000);

    return (sec * 1000) + ms;
}
EXPORT_SYMBOL(usbpd_check_timer_vdm);

void usbpd_timer1_start(struct usbpd_data *pd_data)
{
	ktime_get_real_ts64(&pd_data->time1);
}
EXPORT_SYMBOL(usbpd_timer1_start);

long long usbpd_check_time1(struct usbpd_data *pd_data)
{
	uint32_t ms = 0;
	uint32_t sec = 0;
	struct timespec64 time;

	ktime_get_real_ts64(&time);

    sec = time.tv_sec - pd_data->time1.tv_sec;
    ms = (uint32_t)((time.tv_nsec - pd_data->time1.tv_nsec) / 1000000);

    pd_data->check_time.tv_sec = time.tv_sec;
    pd_data->check_time.tv_nsec = time.tv_nsec;


    /* for demon update after boot */
    if ((sec > 100) || (sec < 0)) {
        pr_info("%s, timer changed\n", __func__);
        usbpd_timer1_start(pd_data);
        return 0;
    }

    return (sec * 1000) + ms;
}
EXPORT_SYMBOL(usbpd_check_time1);

void usbpd_timer2_start(struct usbpd_data *pd_data)
{
	ktime_get_real_ts64(&pd_data->time2);
}
EXPORT_SYMBOL(usbpd_timer2_start);

long long usbpd_check_time2(struct usbpd_data *pd_data)
{
    int ms = 0;
    int sec = 0;
    struct timespec64 time;

    ktime_get_real_ts64(&time);

    sec = time.tv_sec - pd_data->time2.tv_sec;
    ms = (time.tv_nsec - pd_data->time2.tv_nsec) / 1000000;

    return (sec * 1000) + ms;
}
EXPORT_SYMBOL(usbpd_check_time2);

static void increase_message_id_counter(struct usbpd_data *pd_data)
{
	pd_data->counter.message_id_counter++;
	pd_data->counter.message_id_counter %= 8;
/*
	if (pd_data->counter.message_id_counter++ > USBPD_nMessageIDCount)
		pd_data->counter.message_id_counter = 0;
*/
}

static void rx_layer_init(struct protocol_data *rx)
{
	int i;

	rx->stored_message_id = USBPD_nMessageIDCount+1;
	rx->msg_header.word = 0;
	rx->state = 0;
	rx->status = DEFAULT_PROTOCOL_NONE;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		rx->data_obj[i].object = 0;
}

static void tx_layer_init(struct protocol_data *tx)
{
	int i;

	tx->stored_message_id = USBPD_nMessageIDCount+1;
	tx->msg_header.word = 0;
	tx->state = 0;
	tx->status = DEFAULT_PROTOCOL_NONE;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		tx->data_obj[i].object = 0;
}

static void tx_discard_message(struct protocol_data *tx)
{
	int i;

	tx->msg_header.word = 0;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		tx->data_obj[i].object = 0;
}

void usbpd_init_protocol(struct usbpd_data *pd_data)
{
	rx_layer_init(&pd_data->protocol_rx);
	tx_layer_init(&pd_data->protocol_tx);
	pd_data->msg_id = USBPD_nMessageIDCount + 1;
}
EXPORT_SYMBOL(usbpd_init_protocol);

void usbpd_init_counters(struct usbpd_data *pd_data)
{
	pr_info("%s: init counter\n", __func__);
	pd_data->counter.retry_counter = 0;
	pd_data->counter.message_id_counter = 0;
	pd_data->counter.caps_counter = 0;
#if 0
	pd_data->counter.hard_reset_counter = 0;
#endif
	pd_data->counter.swap_hard_reset_counter = 0;
	pd_data->counter.discover_identity_counter = 0;
}
EXPORT_SYMBOL(usbpd_init_counters);

void usbpd_policy_reset(struct usbpd_data *pd_data, unsigned flag)
{
	if (flag == HARDRESET_RECEIVED) {
		pd_data->policy.rx_hardreset = 1;
		dev_info(pd_data->dev, "%s Hard reset\n", __func__);
		usbpd_manager_remove_new_cap(pd_data);
	} else if (flag == SOFTRESET_RECEIVED) {
		pd_data->policy.rx_softreset = 1;
		dev_info(pd_data->dev, "%s Soft reset\n", __func__);
	} else if (flag == PLUG_EVENT) {
		if (!pd_data->policy.plug_valid)
			pd_data->policy.plug = 1;
		pd_data->policy.plug_valid = 1;
		dev_info(pd_data->dev, "%s ATTACHED\n", __func__);
	} else if (flag == PLUG_DETACHED) {
		pd_data->policy.plug_valid = 0;
		dev_info(pd_data->dev, "%s DETACHED\n", __func__);
		pd_data->counter.hard_reset_counter = 0;
		pd_data->source_get_sink_obj.object = 0;
		usbpd_manager_remove_new_cap(pd_data);
	}
}
EXPORT_SYMBOL(usbpd_policy_reset);

protocol_state usbpd_protocol_tx_phy_layer_reset(struct protocol_data *tx)
{
	return PRL_Tx_Wait_for_Message_Request;
}

protocol_state usbpd_protocol_tx_wait_for_message_request(struct protocol_data
								*tx)
{
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);
	protocol_state state = PRL_Tx_Wait_for_Message_Request;

	/* S2MM004 PDIC already retry.
	if (pd_data->counter.retry_counter > USBPD_nRetryCount) {
		pd_data->counter.retry_counter = 0;
		return state;
	}
	*/
	if (pd_data->counter.retry_counter > 0) {
		pd_data->counter.retry_counter = 0;
		return state;
	}

	pd_data->counter.retry_counter = 0;

	if (!tx->msg_header.word)
		return state;

	if (tx->msg_header.num_data_objs == 0 &&
			tx->msg_header.msg_type == USBPD_Soft_Reset)
		state = PRL_Tx_Layer_Reset_for_Transmit;
	else
		state = PRL_Tx_Construct_Message;

	return state;
}

protocol_state usbpd_protocol_tx_layer_reset_for_transmit(struct protocol_data *tx)
{
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);

	dev_info(pd_data->dev, "%s\n", __func__);

	pd_data->counter.message_id_counter = 0;
	pd_data->protocol_rx.state = PRL_Rx_Wait_for_PHY_Message;

	/* TODO: check Layer Reset Complete */
	return PRL_Tx_Construct_Message;
}

protocol_state usbpd_protocol_tx_construct_message(struct protocol_data *tx)
{
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);

	tx->msg_header.msg_id = pd_data->counter.message_id_counter;
	tx->status = DEFAULT_PROTOCOL_NONE;

	if (PDIC_OPS_PARAM2_FUNC(tx_msg, pd_data, &tx->msg_header, tx->data_obj)) {
		dev_err(pd_data->dev, "%s error\n", __func__);
		return PRL_Tx_Construct_Message;
	}
	return PRL_Tx_Wait_for_PHY_Response;
}

protocol_state usbpd_protocol_tx_wait_for_phy_response(struct protocol_data *tx)
{
#if 0
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);
	protocol_state state = PRL_Tx_Wait_for_PHY_Response;
	u8 CrcCheck_cnt = 0;

	/* wait to get goodcrc */
	/* mdelay(1); */

	/* polling */
	/* pd_data->phy_ops.poll_status(pd_data); */

	for (CrcCheck_cnt = 0; CrcCheck_cnt < 2; CrcCheck_cnt++) {
		if (PDIC_OPS_PARAM_FUNC(get_status, pd_data, MSG_GOODCRC)) {
			pr_info("%s : %p\n", __func__, pd_data);
			state = PRL_Tx_Message_Sent;
			dev_info(pd_data->dev, "got GoodCRC.\n");
			return state;
		}

		if (!CrcCheck_cnt)
			pd_data->phy_ops.poll_status(pd_data); /* polling */
	}

	return PRL_Tx_Check_RetryCounter;
#endif
	return PRL_Tx_Message_Sent;
}

protocol_state usbpd_protocol_tx_match_messageid(struct protocol_data *tx)
{
	/* We don't use this function.
	   S2MM004 PDIC already check message id for incoming GoodCRC.

	struct usbpd_data *pd_data = protocol_tx_to_usbpd(protocol_tx);
	protocol_state state = PRL_Tx_Match_MessageID;

	dev_info(pd_data->dev, "%s\n",__func__);

	if (pd_data->protocol_rx.msg_header.msg_id
			== pd_data->counter.message_id_counter)
		state = PRL_Tx_Message_Sent;
	else
		state = PRL_Tx_Check_RetryCounter;

	return state;
	*/
	return PRL_Tx_Message_Sent;
}

protocol_state usbpd_protocol_tx_message_sent(struct protocol_data *tx)
{
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);

	increase_message_id_counter(pd_data);
	tx->status = MESSAGE_SENT;
	/* clear protocol header buffer */
	tx->msg_header.word = 0;

	return PRL_Tx_Wait_for_Message_Request;
}

protocol_state usbpd_protocol_tx_check_retrycounter(struct protocol_data *tx)
{
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);

	/* S2MM004 PDIC already do retry.
	   Driver SW doesn't do retry.

	if (++pd_data->counter.retry_counter > USBPD_nRetryCount) {
		state = PRL_Tx_Transmission_Error;
	} else {
		state = PRL_Tx_Construct_Message;
	}

	return PRL_Tx_Check_RetryCounter;
	*/
	++pd_data->counter.retry_counter;
	return PRL_Tx_Transmission_Error;
}

protocol_state usbpd_protocol_tx_transmission_error(struct protocol_data *tx)
{
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);

	dev_err(pd_data->dev, "%s\n", __func__);

	increase_message_id_counter(pd_data);
	tx->status = TRANSMISSION_ERROR;

	return PRL_Tx_Wait_for_Message_Request;
}

protocol_state usbpd_protocol_tx_discard_message(struct protocol_data *tx)
{
	/* This state is for Only Ping message */
	struct usbpd_data *pd_data = protocol_tx_to_usbpd(tx);

	dev_err(pd_data->dev, "%s\n", __func__);
	tx_discard_message(tx);
	increase_message_id_counter(pd_data);

	return PRL_Tx_PHY_Layer_Reset;
}

void usbpd_set_ops(struct device *dev, usbpd_phy_ops_type *ops)
{
	struct usbpd_data *pd_data = (struct usbpd_data *) dev_get_drvdata(dev);

	pd_data->phy_ops.tx_msg = ops->tx_msg;
	pd_data->phy_ops.rx_msg = ops->rx_msg;
	pd_data->phy_ops.hard_reset = ops->hard_reset;
	pd_data->phy_ops.soft_reset = ops->soft_reset;
	pd_data->phy_ops.set_power_role = ops->set_power_role;
	pd_data->phy_ops.get_power_role = ops->get_power_role;
	pd_data->phy_ops.set_data_role = ops->set_data_role;
	pd_data->phy_ops.get_data_role = ops->get_data_role;
	pd_data->phy_ops.get_vconn_source = ops->get_vconn_source;
	pd_data->phy_ops.set_vconn_source = ops->set_vconn_source;
	pd_data->phy_ops.get_status = ops->get_status;
	pd_data->phy_ops.poll_status = ops->poll_status;
	pd_data->phy_ops.driver_reset = ops->driver_reset;
	pd_data->phy_ops.set_otg_control = ops->set_otg_control;
	pd_data->phy_ops.get_vbus_short_check = ops->get_vbus_short_check;
	pd_data->phy_ops.pd_vbus_short_check = ops->pd_vbus_short_check;
	pd_data->phy_ops.set_pd_control = ops->set_pd_control;
	pd_data->phy_ops.get_side_check = ops->get_side_check;
	pd_data->phy_ops.pr_swap = ops->pr_swap;
	pd_data->phy_ops.vbus_on_check = ops->vbus_on_check;
	pd_data->phy_ops.set_rp_control = ops->set_rp_control;
	pd_data->phy_ops.set_chg_lv_mode = ops->set_chg_lv_mode;
#if IS_ENABLED(CONFIG_TYPEC)
	pd_data->phy_ops.set_pwr_opmode = ops->set_pwr_opmode;
#endif
	pd_data->phy_ops.pd_instead_of_vbus = ops->pd_instead_of_vbus;
	pd_data->phy_ops.op_mode_clear = ops->op_mode_clear;

#if IS_ENABLED(CONFIG_PDIC_PD30)
	pd_data->phy_ops.pps_enable = ops->pps_enable;
	pd_data->phy_ops.get_pps_enable = ops->get_pps_enable;
	pd_data->phy_ops.get_pps_voltage = ops->get_pps_voltage;
	pd_data->phy_ops.send_psrdy = ops->send_psrdy;
	pd_data->phy_ops.send_hard_reset_dc = ops->send_hard_reset_dc;
	pd_data->phy_ops.force_pps_disable = ops->force_pps_disable;
#endif
	pd_data->phy_ops.get_lpm_mode = ops->get_lpm_mode;
	pd_data->phy_ops.set_lpm_mode = ops->set_lpm_mode;
	pd_data->phy_ops.set_normal_mode = ops->set_normal_mode;
	pd_data->phy_ops.get_rid = ops->get_rid;
	pd_data->phy_ops.control_option_command = ops->control_option_command;
#if defined(CONFIG_SEC_FACTORY)
	pd_data->phy_ops.power_off_water_check = ops->power_off_water_check;
#endif
	pd_data->phy_ops.get_water_detect = ops->get_water_detect;
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
	pd_data->phy_ops.water_get_power_role	= ops->water_get_power_role;
	pd_data->phy_ops.ops_water_check		= ops->ops_water_check;
	pd_data->phy_ops.ops_dry_check			= ops->ops_dry_check;
	pd_data->phy_ops.water_opmode			= ops->water_opmode;
#endif
	pd_data->phy_ops.authentic			= ops->authentic;
	pd_data->phy_ops.energy_now			= ops->energy_now;
	pd_data->phy_ops.set_usbpd_reset		= ops->set_usbpd_reset;
	pd_data->phy_ops.ops_get_fsm_state		= ops->ops_get_fsm_state;
	pd_data->phy_ops.set_is_otg_vboost		= ops->set_is_otg_vboost;
	pd_data->phy_ops.get_detach_valid		= ops->get_detach_valid;
	pd_data->phy_ops.rprd_mode_change		= ops->rprd_mode_change;
	pd_data->phy_ops.irq_control			= ops->irq_control;

	pd_data->phy_ops.ops_get_lpm_mode		= ops->ops_get_lpm_mode;
	pd_data->phy_ops.ops_get_rid			= ops->ops_get_rid;
	pd_data->phy_ops.ops_sysfs_lpm_mode		= ops->ops_sysfs_lpm_mode;
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
	pd_data->phy_ops.ops_power_off_water	= ops->ops_power_off_water;
	pd_data->phy_ops.ops_get_is_water_detect	= ops->ops_get_is_water_detect;
	pd_data->phy_ops.ops_prt_water_threshold	= ops->ops_prt_water_threshold;
	pd_data->phy_ops.ops_set_water_threshold	= ops->ops_set_water_threshold;
#endif
	pd_data->phy_ops.ops_control_option_command	= ops->ops_control_option_command;
	pd_data->phy_ops.set_pcp_clk			= ops->set_pcp_clk;
}
EXPORT_SYMBOL(usbpd_set_ops);

protocol_state usbpd_protocol_rx_layer_reset_for_receive(struct protocol_data *rx)
{
	struct usbpd_data *pd_data = protocol_rx_to_usbpd(rx);

	dev_info(pd_data->dev, "%s\n", __func__);
	/*
	rx_layer_init(protocol_rx);
	pd_data->protocol_tx.state = PRL_Tx_PHY_Layer_Reset;

	usbpd_rx_soft_reset(pd_data);
	*/
	return PRL_Rx_Layer_Reset_for_Receive;

	/*return PRL_Rx_Send_GoodCRC;*/
}

protocol_state usbpd_protocol_rx_wait_for_phy_message(struct protocol_data *rx)
{
	struct usbpd_data *pd_data = protocol_rx_to_usbpd(rx);
	protocol_state state = PRL_Rx_Wait_for_PHY_Message;

	if (PDIC_OPS_PARAM2_FUNC(rx_msg, pd_data, &rx->msg_header, rx->data_obj)) {
		dev_err(pd_data->dev, "%s IO Error\n", __func__);
		return state;
	} else {
		if (rx->msg_header.word == 0) {
			dev_err(pd_data->dev, "%s No Message\n", __func__);
			return state; /* no message */
		} else if (PDIC_OPS_PARAM_FUNC(get_status, pd_data, MSG_SOFTRESET))	{
			dev_err(pd_data->dev, "[Rx] Got SOFTRESET.\n");
			state = PRL_Rx_Layer_Reset_for_Receive;
		} else {
#if 0
			if (rx->stored_message_id == rx->msg_header.msg_id)
				return state;
#endif
			dev_err(pd_data->dev, "[Rx] [0x%x] [0x%x]\n",
					rx->msg_header.word, rx->data_obj[0].object);
			/* new message is coming */
			state = PRL_Rx_Send_GoodCRC;
		}
	}
	return state;
}

protocol_state usbpd_protocol_rx_send_goodcrc(struct protocol_data *rx)
{
	/* Goodcrc sent by PDIC(HW) */
	return PRL_Rx_Check_MessageID;
}

protocol_state usbpd_protocol_rx_store_messageid(struct protocol_data *rx)
{
	struct usbpd_data *pd_data = protocol_rx_to_usbpd(rx);

	rx->stored_message_id = rx->msg_header.msg_id;
	usbpd_read_msg(pd_data);
/*
	return PRL_Rx_Wait_for_PHY_Message;
*/
	return PRL_Rx_Store_MessageID;
}

protocol_state usbpd_protocol_rx_check_messageid(struct protocol_data *rx)
{
#if 0
	protocol_state state;

	if (rx->stored_message_id == rx->msg_header.msg_id)
		state = PRL_Rx_Wait_for_PHY_Message;
	else
		state = PRL_Rx_Store_MessageID;
	return state;
#endif
	return PRL_Rx_Store_MessageID;
}

void usbpd_protocol_tx(struct usbpd_data *pd_data)
{
	struct protocol_data *tx = &pd_data->protocol_tx;
	protocol_state next_state = tx->state;
	protocol_state saved_state;

	do {
		saved_state = next_state;
		switch (next_state) {
		case PRL_Tx_PHY_Layer_Reset:
			next_state = usbpd_protocol_tx_phy_layer_reset(tx);
			break;
		case PRL_Tx_Wait_for_Message_Request:
			next_state = usbpd_protocol_tx_wait_for_message_request(tx);
			break;
		case PRL_Tx_Layer_Reset_for_Transmit:
			next_state = usbpd_protocol_tx_layer_reset_for_transmit(tx);
			break;
		case PRL_Tx_Construct_Message:
			next_state = usbpd_protocol_tx_construct_message(tx);
			break;
		case PRL_Tx_Wait_for_PHY_Response:
			next_state = usbpd_protocol_tx_wait_for_phy_response(tx);
			break;
		case PRL_Tx_Match_MessageID:
			next_state = usbpd_protocol_tx_match_messageid(tx);
			break;
		case PRL_Tx_Message_Sent:
			next_state = usbpd_protocol_tx_message_sent(tx);
			break;
		case PRL_Tx_Check_RetryCounter:
			next_state = usbpd_protocol_tx_check_retrycounter(tx);
			break;
		case PRL_Tx_Transmission_Error:
			next_state = usbpd_protocol_tx_transmission_error(tx);
			break;
		case PRL_Tx_Discard_Message:
			next_state = usbpd_protocol_tx_discard_message(tx);
			break;
		default:
			next_state = PRL_Tx_Wait_for_Message_Request;
			break;
		}
	} while (saved_state != next_state);

	tx->state = next_state;
}

void usbpd_protocol_rx(struct usbpd_data *pd_data)
{
	struct protocol_data *rx = &pd_data->protocol_rx;
	protocol_state next_state = rx->state;
	protocol_state saved_state;

	do {
		saved_state = next_state;
		switch (next_state) {
		case PRL_Rx_Layer_Reset_for_Receive:
			next_state = usbpd_protocol_rx_layer_reset_for_receive(rx);
			break;
		case PRL_Rx_Wait_for_PHY_Message:
			next_state = usbpd_protocol_rx_wait_for_phy_message(rx);
			break;
		case PRL_Rx_Send_GoodCRC:
			next_state = usbpd_protocol_rx_send_goodcrc(rx);
			break;
		case PRL_Rx_Store_MessageID:
			next_state = usbpd_protocol_rx_store_messageid(rx);
			break;
		case PRL_Rx_Check_MessageID:
			next_state = usbpd_protocol_rx_check_messageid(rx);
			break;
		default:
			next_state = PRL_Rx_Wait_for_PHY_Message;
			break;
		}
	} while (saved_state != next_state);
/*
	rx->state = next_state;
*/
	rx->state = PRL_Rx_Wait_for_PHY_Message;
}
EXPORT_SYMBOL(usbpd_protocol_rx);

void usbpd_read_msg(struct usbpd_data *pd_data)
{
	int i;

	pd_data->policy.rx_msg_header.word
		= pd_data->protocol_rx.msg_header.word;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++) {
		pd_data->policy.rx_data_obj[i].object
			= pd_data->protocol_rx.data_obj[i].object;
	}
}

/* return 1: sent with goodcrc, 0: fail */
bool usbpd_send_msg(struct usbpd_data *pd_data, msg_header_type *header,
		data_obj_type *obj)
{
	int i;

	if (obj)
		for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
			pd_data->protocol_tx.data_obj[i].object = obj[i].object;
	else
		header->num_data_objs = 0;
	header->spec_revision = pd_data->specification_revision;
	pd_data->protocol_tx.msg_header.word = header->word;
	usbpd_protocol_tx(pd_data);

	if (pd_data->protocol_tx.status == MESSAGE_SENT)
		return true;
	else
		return false;
}
EXPORT_SYMBOL(usbpd_send_msg);

inline bool usbpd_send_ctrl_msg(struct usbpd_data *d, msg_header_type *h,
		unsigned msg, unsigned dr, unsigned pr)
{
	h->msg_type = msg;
	h->port_data_role = dr;
	h->port_power_role = pr;
	h->num_data_objs = 0;
	return usbpd_send_msg(d, h, 0);
}
EXPORT_SYMBOL(usbpd_send_ctrl_msg);

/* return: 0 if timed out, positive is status */
inline unsigned usbpd_wait_msg(struct usbpd_data *pd_data,
				unsigned msg_status, unsigned ms)
{
	unsigned long ret;

	ret = PDIC_OPS_PARAM_FUNC(get_status, pd_data, msg_status);
	if (ret) {
		pd_data->policy.abnormal_state = false;
		return ret;
	}

	pr_info("%s, %d\n", __func__, __LINE__);
	/* wait */
	reinit_completion(&pd_data->msg_arrived);
	pd_data->wait_for_msg_arrived = msg_status;
	ret = wait_for_completion_timeout(&pd_data->msg_arrived,
			msecs_to_jiffies(ms));

	if (pd_data->policy.state == 0) {
		dev_err(pd_data->dev, "%s : return for policy state error\n", __func__);
		pd_data->policy.abnormal_state = true;
		return 0;
	}

	pd_data->policy.abnormal_state = false;

	return PDIC_OPS_PARAM_FUNC(get_status, pd_data, msg_status);
}

void usbpd_rx_hard_reset(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);

	usbpd_reinit(dev);
	usbpd_policy_reset(pd_data, HARDRESET_RECEIVED);
}
EXPORT_SYMBOL(usbpd_rx_hard_reset);

void usbpd_rx_soft_reset(struct usbpd_data *pd_data)
{
	usbpd_reinit(pd_data->dev);
	usbpd_policy_reset(pd_data, SOFTRESET_RECEIVED);
}

void usbpd_reinit(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);

	usbpd_init_counters(pd_data);
	usbpd_init_protocol(pd_data);
	usbpd_init_policy(pd_data);
	usbpd_init_manager_val(pd_data);
	reinit_completion(&pd_data->msg_arrived);
	pd_data->wait_for_msg_arrived = 0;
	pd_data->is_prswap = false;
	complete(&pd_data->msg_arrived);
}
EXPORT_SYMBOL(usbpd_reinit);

/*
 * usbpd_init - alloc usbpd data
 *
 * Returns 0 on success; negative errno on failure
*/
int usbpd_sysfs_get_prop(struct _pdic_data_t *ppdic_data,
		enum pdic_sysfs_property prop, char *buf)
{
	struct usbpd_data *pd_data = ppdic_data->drv_data;
	struct usbpd_manager_data *manager;
	int retval = -ENODEV;
	int var;

	if (!pd_data) {
		pr_info("pd_data is null : request prop = %d", prop);
		return -ENODEV;
	}

	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s manager_data is null!!\n", __func__);
		return -ENODEV;
	}

	switch (prop) {
	case PDIC_SYSFS_PROP_CUR_VERSION:
		pr_info("Need implementation\n");
		break;
	case PDIC_SYSFS_PROP_SRC_VERSION:
		pr_info("Need implementation\n");
		break;
	case PDIC_SYSFS_PROP_LPM_MODE:
		retval = sprintf(buf, "%d\n", PDIC_OPS_FUNC(ops_get_lpm_mode, pd_data));
		break;
	case PDIC_SYSFS_PROP_STATE:
		retval = sprintf(buf, "%d\n", pd_data->policy.plug_valid);
		break;
	case PDIC_SYSFS_PROP_RID:
		var = PDIC_OPS_FUNC(ops_get_rid, pd_data);
		retval = sprintf(buf, "%d\n", var == REG_RID_MAX ? REG_RID_OPEN : var);
		break;
	case PDIC_SYSFS_PROP_BOOTING_DRY:
		pr_info("%s booting_run_dry is not supported \n", __func__);
		break;
	case PDIC_SYSFS_PROP_FW_UPDATE_STATUS:
		pr_info("Need implementation\n");
		break;
	case PDIC_SYSFS_PROP_FW_WATER:
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
		var = PDIC_OPS_FUNC(ops_get_is_water_detect, pd_data);
		pr_info("%s is_water_detect=%d\n", __func__, var);
		retval = sprintf(buf, "%d\n", var);
#endif
		break;
	case PDIC_SYSFS_PROP_ACC_DEVICE_VERSION:
		pr_info("khos %s 0x%04x\n", __func__, manager->Device_Version);
		retval = sprintf(buf, "%04x\n", manager->Device_Version);
		break;
	case PDIC_SYSFS_PROP_USBPD_IDS:
		retval = sprintf(buf, "%04x:%04x\n",
			le16_to_cpu(manager->Vendor_ID),
			le16_to_cpu(manager->Product_ID));
		break;
	case PDIC_SYSFS_PROP_USBPD_TYPE:	/* for SWITCH_STATE */
		retval = sprintf(buf, "%d\n", manager->acc_type);
		pr_info("usb: %s : %d",
			__func__, manager->acc_type);
		break;
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
	case PDIC_SYSFS_PROP_USBPD_WATER_CHECK:
		retval = PDIC_OPS_FUNC(ops_power_off_water, pd_data);
		sprintf(buf, "%d\n", retval);
		break;
#endif
	case PDIC_SYSFS_PROP_SET_WATER_THRESHOLD:
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
		pr_info("%s, WATER_THRESHOLD\n", __func__);
		retval = PDIC_OPS_PARAM_FUNC(ops_prt_water_threshold, pd_data, buf);
#endif
		break;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	case PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE:
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
		retval = sprintf(buf, "uevent\n");
#else
		retval = sprintf(buf, "unsupport\n");
#endif
		pr_info("%s : PDIC_SYSFS_PROP_15MODE_WATERTEST_TYPE : %s", __func__, buf);
		break;
#endif
	default:
		pr_info("prop read not supported prop (%d)", prop);
		retval = -ENODATA;
		break;
	}

	return retval;
}
EXPORT_SYMBOL_GPL(usbpd_sysfs_get_prop);


ssize_t usbpd_sysfs_set_prop(struct _pdic_data_t *ppdic_data,
				enum pdic_sysfs_property prop,
				const char *buf, size_t size)
{
	struct usbpd_data *pd_data = ppdic_data->drv_data;
	struct usbpd_manager_data *manager;
	int retval = size, cmd;
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
	int val1, val2;
#endif

	if (!pd_data) {
		pr_info("pd_data is null : request prop = %d", prop);
		return -ENODEV;
	}

	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s manager_data is null!!\n", __func__);
		return -ENODEV;
	}

	switch (prop) {
	case PDIC_SYSFS_PROP_SET_WATER_THRESHOLD:
#if IS_ENABLED(CONFIG_S2MU106_TYPEC_WATER)
		sscanf(buf, "%d %d", &val1, &val2);
		pr_info("%s, PROP_SET_WATER_THRESHOLD buf : %s\n", __func__, buf);
		pr_info("%s, #%d is set to %d\n", __func__, val1, val2);
		PDIC_OPS_PARAM2_FUNC(ops_set_water_threshold, pd_data, val1, val2);
#endif
		break;
	case PDIC_SYSFS_PROP_CTRL_OPTION:
		sscanf(buf, "%d", &cmd);
		pr_info("usb: %s PROP_CTRL_OPTION mode=%d\n", __func__, cmd);
		PDIC_OPS_PARAM_FUNC(ops_control_option_command, pd_data, cmd);
		break;
	case PDIC_SYSFS_PROP_LPM_MODE:
		sscanf(buf, "%d", &cmd);
		pr_info("usb: %s PROP_LPM_MODE mode=%d\n", __func__, cmd);
		PDIC_OPS_PARAM_FUNC(ops_sysfs_lpm_mode, pd_data, cmd);
		break;
	default:
		pr_info("prop read not supported prop (%d)", prop);
		retval = -ENODATA;
		break;
	}

	return retval;
}
EXPORT_SYMBOL_GPL(usbpd_sysfs_set_prop);

int usbpd_sysfs_is_writeable(struct _pdic_data_t *ppdic_data,
				enum pdic_sysfs_property prop)
{
	int ret = 0;

	switch (prop) {
	case PDIC_SYSFS_PROP_CTRL_OPTION:
	case PDIC_SYSFS_PROP_SET_WATER_THRESHOLD:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(usbpd_sysfs_is_writeable);

int usbpd_init(struct device *dev, void *phy_driver_data)
{
	struct usbpd_data *pd_data;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	int ret = 0;
	ppdic_data_t ppdic_data;
	struct device *pdic_device = get_pdic_device();
	ppdic_sysfs_property_t ppdic_sysfs_prop;
#endif

	if (!dev)
		return -EINVAL;

	pd_data = kzalloc(sizeof(struct usbpd_data), GFP_KERNEL);

	if (!pd_data)
		return -ENOMEM;

	pd_data->dev = dev;
	pd_data->phy_driver_data = phy_driver_data;
	dev_set_drvdata(dev, pd_data);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	g_pd_data = pd_data;
	pd_data->pd_noti.pusbpd = pd_data;
	pd_data->pd_noti.sink_status.current_pdo_num = 0;
	pd_data->pd_noti.sink_status.selected_pdo_num = 0;
#endif
	usbpd_init_counters(pd_data);
	usbpd_init_protocol(pd_data);
	usbpd_init_policy(pd_data);
	usbpd_init_manager(pd_data);

	mutex_init(&pd_data->accept_mutex);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 188)
	wakeup_source_init(pd_data->policy_wake, "policy_wake");   // 4.19 R
	if (!(pd_data->policy_wake)) {
		pd_data->policy_wake = wakeup_source_create("policy_wake"); // 4.19 Q
		if (pd_data->policy_wake)
			wakeup_source_add(pd_data->policy_wake);
	}
#else
	pd_data->policy_wake = wakeup_source_register(NULL, "policy_wake"); // 5.4 R
#endif

	pd_data->policy_wqueue =
		create_singlethread_workqueue(dev_name(dev));
	if (!pd_data->policy_wqueue)
		pr_err("%s: Fail to Create Workqueue\n", __func__);

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	dev_set_drvdata(pdic_device, pd_data);
	ppdic_data = kzalloc(sizeof(pdic_data_t), GFP_KERNEL);
	ppdic_sysfs_prop = kzalloc(sizeof(pdic_sysfs_property_t), GFP_KERNEL);
	ppdic_sysfs_prop->get_property = usbpd_sysfs_get_prop;
	ppdic_sysfs_prop->set_property = usbpd_sysfs_set_prop;
	ppdic_sysfs_prop->property_is_writeable = usbpd_sysfs_is_writeable;
	ppdic_sysfs_prop->properties = usbpd_sysfs_properties;
	ppdic_sysfs_prop->num_properties = ARRAY_SIZE(usbpd_sysfs_properties);
	ppdic_data->pdic_sysfs_prop = ppdic_sysfs_prop;
	ppdic_data->drv_data = pd_data;
	ppdic_data->name = "s2m_pdic";
	pdic_core_register_chip(ppdic_data);
	pdic_register_switch_device(1);

	/* Create a work queue for the pdic irq thread */
	pd_data->pdic_wq
		= create_singlethread_workqueue("pdic_irq_event");
	if (!pd_data->pdic_wq) {
		pr_err("%s failed to create work queue for pdic notifier\n",
			__func__);
		return -ENOMEM;
	}
	ret = pdic_misc_init(ppdic_data);
	if (ret) {
		pr_err("pdic_misc_init is failed, error %d\n", ret);
	} else {
		ppdic_data->misc_dev->uvdm_ready = samsung_uvdm_ready;
		ppdic_data->misc_dev->uvdm_close = samsung_uvdm_close;
		ppdic_data->misc_dev->uvdm_read = samsung_uvdm_read;
		ppdic_data->misc_dev->uvdm_write = samsung_uvdm_write;
		pd_data->ppdic_data = ppdic_data;
	}
#endif

	INIT_WORK(&pd_data->worker, usbpd_policy_work);

	init_completion(&pd_data->msg_arrived);

	return 0;
}
EXPORT_SYMBOL(usbpd_init);

