#include <linux/device.h>
#include <linux/module.h>

#include <linux/notifier.h>
#include <linux/ifconn/ifconn_notifier.h>
#include <linux/ifconn/ifconn_manager.h>
#if defined(CONFIG_PDIC_SUPPORT_DP)
#include <linux/power_supply.h>
#include <linux/power/s2m_chg_manager.h>
#endif

#define DEBUG
#define SET_IFCONN_NOTIFIER_BLOCK(nb, fn, dev) do {	\
		(nb)->notifier_call = (fn);		\
		(nb)->priority = (dev);			\
} while (0)

#define DESTROY_IFCONN_NOTIFIER_BLOCK(nb)			\
		SET_IFCONN_NOTIFIER_BLOCK(nb, NULL, -1)

static char IFCONN_NOTI_DEV_Print[IFCONN_NOTIFY_MAX][15] = {
	{"MANAGER"},
	{"USB"},
	{"BATTERY"},
	{"PDIC"},
	{"MUIC"},
	{"VBUS"},
	{"CCIC"},
	{"DP"},
	{"DPUSB"},
	{"MODEM"},
	{"FLED"},
	{"MANAGER_MUIC"},
	{"MANAGER_PDIC"},
	{"ALL"},
};

void (*fp_select_pdo)(int num);
int (*fp_select_pps)(int num, int ppsVol, int ppsCur);
int (*fp_pd_get_apdo_max_power)(unsigned int *pdo_pos, unsigned int *taMaxVol,
		unsigned int *taMaxCur, unsigned int *taMaxPwr);
int (*fp_pd_pps_enable)(int num, int ppsVol, int ppsCur, int enable);
int (*fp_pd_get_pps_voltage)(void);
EXPORT_SYMBOL_GPL(fp_select_pdo);
EXPORT_SYMBOL_GPL(fp_select_pps);
EXPORT_SYMBOL_GPL(fp_pd_get_apdo_max_power);
EXPORT_SYMBOL_GPL(fp_pd_pps_enable);
EXPORT_SYMBOL_GPL(fp_pd_get_pps_voltage);

void select_pdo(int num)
{
	if (fp_select_pdo)
		fp_select_pdo(num);
}

int select_pps(int num, int ppsVol, int ppsCur)
{
	if (fp_select_pps)
		fp_select_pps(num, ppsVol, ppsCur);

	return 0;
}

int pd_get_apdo_max_power(unsigned int *pdo_pos, unsigned int *taMaxVol,
		unsigned int *taMaxCur, unsigned int *taMaxPwr)
{
	if (fp_pd_get_apdo_max_power)
		fp_pd_get_apdo_max_power(pdo_pos, taMaxVol, taMaxCur, taMaxPwr);

	return 0;
}

int pd_pps_enable(int num, int ppsVol, int ppsCur, int enable)
{
	if (fp_pd_pps_enable)
		fp_pd_pps_enable(num, ppsVol, ppsCur, enable);

	return 0;
}

int pd_get_pps_voltage(void)
{
	if (fp_pd_get_pps_voltage)
		return fp_pd_get_pps_voltage();
	return 0;
}
EXPORT_SYMBOL_GPL(select_pdo);
EXPORT_SYMBOL_GPL(select_pps);
EXPORT_SYMBOL_GPL(pd_get_apdo_max_power);
EXPORT_SYMBOL_GPL(pd_pps_enable);
EXPORT_SYMBOL_GPL(pd_get_pps_voltage);



static struct ifconn_notifier pnoti[IFCONN_NOTIFY_MAX];

void _ifconn_show_attr(struct ifconn_notifier_template *t)
{
	if (t == NULL)
		return;

	pr_info("%s, src:%s, dest:%s, id:%d, event:%d, data:0x%p\n",
		__func__, IFCONN_NOTI_DEV_Print[t->src], IFCONN_NOTI_DEV_Print[t->dest], t->id, t->event, t->data);
}

int ifconn_notifier_register(struct notifier_block *nb, notifier_fn_t notifier,
			     ifconn_notifier_t listener, ifconn_notifier_t src)
{
	int ret = 0;
	struct ifconn_notifier_template *template = NULL;
#if defined(CONFIG_PDIC_SUPPORT_DP)
	union power_supply_propval value;
	struct power_supply *psy;
#endif

	pr_info("%s: src : %d =? listner : %d, nb : %p  register\n", __func__,
		src, listener, nb);

	if (src >= IFCONN_NOTIFY_ALL || src < 0
	    || listener >= IFCONN_NOTIFY_ALL || listener < 0) {
		pr_err("%s: dev index err\n", __func__);
		return -1;
	}

	SET_IFCONN_NOTIFIER_BLOCK(nb, notifier, listener);
	ret = blocking_notifier_chain_register(&(pnoti[src].notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_register error(%d)\n",
		       __func__, ret);

	pnoti[src].nb[listener] = nb;
	if (pnoti[src].sent[listener] || pnoti[src].sent[IFCONN_NOTIFY_ALL]) {
		pr_info("%s: src : %d, listner : %d, sent : %d, sent all : %d\n", __func__,
			src, listener, pnoti[src].sent[listener], pnoti[src].sent[IFCONN_NOTIFY_ALL]);

		if (!pnoti[src].sent[listener] && pnoti[src].sent[IFCONN_NOTIFY_ALL]) {
			template = &pnoti[src].ifconn_template[IFCONN_NOTIFY_ALL];
			_ifconn_show_attr(template);
			template->src = src;
			template->dest = listener;
		} else {
			template = &(pnoti[src].ifconn_template[listener]);
		}
		if (listener == IFCONN_NOTIFY_BATTERY) {
			if (ifconn_manager_get_pd_con_state()) {
				pr_info("%s, PD connected\n", __func__);
			} else {
				pr_info("%s, PD not connected, muic notify\n", __func__);
				template->id = IFCONN_NOTIFY_ID_ATTACH;
				template->attach = IFCONN_NOTIFY_ATTACH;
				template->cable_type = ifconn_manager_get_muic_cable_type();
				template->event = ifconn_manager_get_muic_cable_type();
			}

		}
		_ifconn_show_attr(template);
		ret = nb->notifier_call(nb,	template->id, template);
	}

#if defined(CONFIG_PDIC_SUPPORT_DP)
	if (listener == IFCONN_NOTIFY_DP) {
		pr_info("%s, DP registered, set to pdic start VDM\n", __func__);
		psy = power_supply_get_by_name("s2mu106-usbpd");
		if (!psy) {
			pr_info("%s, fail to get psy\n", __func__);
			psy = power_supply_get_by_name("usbpd-manager");
			if (!psy) {
				pr_info("%s, fail to get psy\n", __func__);
				return ret;
			}
		}

		value.intval = 1;
		power_supply_set_property(psy, (enum power_supply_property)POWER_SUPPLY_S2M_PROP_DP_ENABLED, &value);
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(ifconn_notifier_register);

int ifconn_notifier_unregister(ifconn_notifier_t src,
			       ifconn_notifier_t listener)
{
	int ret = 0;
	struct notifier_block *nb = NULL;

	if (src >= IFCONN_NOTIFY_ALL || src < 0
	    || listener >= IFCONN_NOTIFY_ALL || listener < 0) {
		pr_err("%s: dev index err\n", __func__);
		return -1;
	}
	nb = pnoti[src].nb[listener];

	pr_info("%s: listener=%d unregister\n", __func__, nb->priority);

	ret = blocking_notifier_chain_unregister(&(pnoti[src].notifier_call_chain), nb);
	if (ret < 0)
		pr_err("%s: blocking_notifier_chain_unregister error(%d)\n",
		       __func__, ret);
	DESTROY_IFCONN_NOTIFIER_BLOCK(nb);

	return ret;
}
EXPORT_SYMBOL_GPL(ifconn_notifier_unregister);

int ifconn_notifier_notify(ifconn_notifier_t src,
			   ifconn_notifier_t listener,
			   int noti_id,
			   int event,
			   ifconn_notifier_data_param_t param_type, void *data)
{
	int ret = 0;
	struct ifconn_notifier_template *template = NULL;
	struct notifier_block *nb = NULL;

	pr_info("%s: enter\n", __func__);

	if (src > IFCONN_NOTIFY_ALL || src < 0
	    || listener > IFCONN_NOTIFY_ALL || listener < 0) {
		pr_err("%s: dev index err\n", __func__);
		return -1;
	}
	pr_info("%s: src = %s, listener = %s, nb = 0x%p notify\n", __func__,
		IFCONN_NOTI_DEV_Print[src], IFCONN_NOTI_DEV_Print[listener],
		pnoti[src].nb[listener]);

	if (param_type == IFCONN_NOTIFY_PARAM_TEMPLATE) {
		template = (struct ifconn_notifier_template *)data;
	} else {
		template = &(pnoti[src].ifconn_template[listener]);
		template->id = (uint64_t) noti_id;
		template->up_src = template->src = (uint64_t) src;
		template->dest = (uint64_t) listener;
		template->cable_type = template->event = event;
		template->data = data;
	}

	pr_info("[DEBUG] %s, sub1(%d), sub2(%d)\n", __func__, template->sub1, template->sub2);

	_ifconn_show_attr(template);

	if (pnoti[src].nb[listener] == NULL)
		pnoti[src].sent[listener] = true;

	if (listener == IFCONN_NOTIFY_ALL) {
		ret = blocking_notifier_call_chain(&(pnoti[src].notifier_call_chain),
				(unsigned long)noti_id, template);
	} else {
		if (pnoti[src].nb[listener] == NULL) {
			if (param_type == IFCONN_NOTIFY_PARAM_TEMPLATE) {
				memcpy(&(pnoti[src].ifconn_template[listener]), template,
					   sizeof(struct ifconn_notifier_template));
			}
			return -1;
		}

		nb = pnoti[src].nb[listener];
		ret = nb->notifier_call(nb, (unsigned long)noti_id, template);
	}

	switch (ret) {
	case NOTIFY_STOP_MASK:
	case NOTIFY_BAD:
		pr_err("%s: notify error occur(0x%x)\n", __func__, ret);
		break;
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s: notify done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s: notify status unknown(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ifconn_notifier_notify);

#if 0
static int __init ifconn_notifier_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);
	return ret;
}

device_initcall(ifconn_notifier_init);
#endif
