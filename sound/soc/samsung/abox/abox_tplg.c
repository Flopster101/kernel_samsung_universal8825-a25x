// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox Topology driver
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>

#include "abox.h"
#include "abox_ipc.h"
#include "abox_dump.h"
#include "abox_dma.h"
#include "abox_vdma.h"
#include "abox_tplg.h"
#include "abox_memlog.h"

#define ABOX_TPLG_DAPM_CTL_VOLSW		0x100
#define ABOX_TPLG_DAPM_CTL_ENUM_DOUBLE		0x101
#define ABOX_TPLG_DAPM_CTL_ENUM_VIRT		0x102
#define ABOX_TPLG_DAPM_CTL_ENUM_VALUE		0x103
#define ABOX_TPLG_DAPM_CTL_PIN			0x104
#define ABOX_TPLG_CTL_VOLSW			0x110
#define ABOX_TPLG_CTL_ENUM_DOUBLE		0x111
#define ABOX_TPLG_CTL_ENUM_VALUE		0x112
#define ABOX_TPLG_CTL_PIPELINE			0x113
#define ABOX_TPLG_CTL_VOLSW_WRITE_ONLY		0x120

#define UNKNOWN_VALUE -1

static const int MIN_VIRTUAL_WIDGET_ID = 0x1000;

static struct device *dev_abox;
static LIST_HEAD(widget_list);
static LIST_HEAD(kcontrol_list);
static LIST_HEAD(dai_list);
static DEFINE_MUTEX(kcontrol_mutex);

struct abox_tplg_widget_data {
	struct list_head list;
	int gid;
	int id;
	unsigned int value;
	bool weak;
	struct snd_soc_component *cmpnt;
	struct snd_soc_dapm_widget *w;
	struct snd_soc_tplg_dapm_widget *tplg_w;
};

struct abox_tplg_pipeline_item {
	struct abox_tplg_kcontrol_data *kdata;
	int value;
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	char text[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	struct snd_soc_tplg_vendor_string_elem *tplg_vse;
	struct list_head work;
};

struct abox_tplg_pipeline {
	unsigned int id;
	unsigned int count;
	const char *name;
	struct snd_soc_tplg_vendor_array *tplg_va;
	struct abox_tplg_pipeline_item *items;
};

struct abox_tplg_pipelines {
	unsigned int count;
	struct abox_tplg_pipeline *pl;
};

struct abox_tplg_kcontrol_data {
	struct list_head list;
	int gid;
	int id;
	unsigned int value[128];
	int count;
	bool is_volatile;
	bool synchronous;
	bool force_restore;
	unsigned int addr;
	unsigned int *kaddr;
	struct abox_tplg_pipelines pls;
	struct snd_soc_component *cmpnt;
	struct snd_kcontrol_new *kcontrol_new;
	struct snd_soc_dobj *dobj;
	struct snd_kcontrol *kcontrol; /* use abox_tplg_get_kcontrol() */
	union {
		struct snd_soc_tplg_ctl_hdr *hdr;
		struct snd_soc_tplg_mixer_control *tplg_mc;
		struct snd_soc_tplg_enum_control *tplg_ec;
	};
};

struct abox_tplg_dai_data {
	struct list_head list;
	int id;
	struct snd_soc_dai_driver *dai_drv;
	struct device *dev_platform;
};

struct abox_tplg_firmware {
	const char * const fw_name;
	const struct firmware *fw;
	bool optional;
};

static struct abox_tplg_firmware abox_tplg_fw[] = {
	{
		.fw_name = "abox_tplg.bin",
	},
	{
		.fw_name = "sectiongraph_tplg.bin",
		.optional = true,
	},
};

static struct abox_tplg_kcontrol_data *abox_tplg_find_kcontrol_data(const char *name)
{
	struct abox_tplg_kcontrol_data *kdata;

	list_for_each_entry(kdata, &kcontrol_list, list) {
		if (!strcmp(kdata->hdr->name, name))
			return kdata;
	}

	return NULL;
}

static struct snd_kcontrol *abox_tplg_get_kcontrol(struct abox_tplg_kcontrol_data *kdata)
{
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dapm_widget *w;
	int i;

	if (kdata->kcontrol)
		return kdata->kcontrol;

	switch (kdata->hdr->ops.info) {
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
		kdata->kcontrol = kdata->dobj->control.kcontrol;
		break;
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
	case SND_SOC_TPLG_DAPM_CTL_PIN:
		dapm = snd_soc_component_get_dapm(cmpnt);
		list_for_each_entry(w, &dapm->card->widgets, list) {
			for (i = 0; i < w->num_kcontrols; i++) {
				if (&w->kcontrol_news[i] == kdata->kcontrol_new) {
					if (w->kcontrols) {
						kdata->kcontrol = w->kcontrols[i];
						break;
					}
				}
			}
		}
		break;
	default:
		abox_warn(dev, "unsupported pipeline item type: %d\n",
				kdata->hdr->name);
		break;
	}

	if (!kdata->kcontrol)
		abox_warn(dev, "can't find kcontrol: %s\n", kdata->hdr->name);

	return kdata->kcontrol;
}

static int abox_tplg_register_dump(struct device *dev, int gid, int id,
		const char *name)
{
	struct abox_data *data = dev_get_drvdata(dev_abox);

	return abox_dump_register(data, gid, id, name, NULL, 0, 0);
}

static int abox_tplg_request_ipc(ABOX_IPC_MSG *msg)
{
	return abox_request_ipc(dev_abox, msg->ipcid, msg, sizeof(*msg), 0, 0);
}

static bool abox_tplg_get_bool_at(struct snd_soc_tplg_private *priv, int token,
		int idx)
{
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_value_elem *value;
	int sz;

	for (sz = 0; sz < priv->size; sz += array->size) {
		array = (struct snd_soc_tplg_vendor_array *)(priv->data + sz);

		if (array->type != SND_SOC_TPLG_TUPLE_TYPE_BOOL)
			continue;

		for (value = array->value; value - array->value <
				array->num_elems; value++) {
			if (value->token == token && !idx--)
				return !!value->value;
		}
	}

	return false;
}

static int abox_tplg_get_int_at(struct snd_soc_tplg_private *priv, int token,
		int idx)
{
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_value_elem *value;
	int sz;

	for (sz = 0; sz < priv->size; sz += array->size) {
		array = (struct snd_soc_tplg_vendor_array *)(priv->data + sz);

		if (array->type != SND_SOC_TPLG_TUPLE_TYPE_WORD)
			continue;

		for (value = array->value; value - array->value <
				array->num_elems; value++) {
			if (value->token == token && !idx--)
				return value->value;
		}
	}

	return -EINVAL;
}

static const char *abox_tplg_get_string_at(struct snd_soc_tplg_private *priv,
		int token, int idx)
{
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_string_elem *string;
	int sz;

	for (sz = 0; sz < priv->size; sz += array->size) {
		array = (struct snd_soc_tplg_vendor_array *)(priv->data + sz);

		if (array->type != SND_SOC_TPLG_TUPLE_TYPE_STRING)
			continue;

		for (string = array->string; string - array->string <
				array->num_elems; string++) {
			if (string->token == token && !idx--)
				return string->string;
		}
	}

	return NULL;
}

static bool abox_tplg_get_bool(struct snd_soc_tplg_private *priv, int token)
{
	return abox_tplg_get_bool_at(priv, token, 0);
}

static int abox_tplg_get_int(struct snd_soc_tplg_private *priv, int token)
{
	return abox_tplg_get_int_at(priv, token, 0);
}

static int abox_tplg_get_id(struct snd_soc_tplg_private *priv)
{
	return abox_tplg_get_int(priv, ABOX_TKN_ID);
}

static int abox_tplg_get_gid(struct snd_soc_tplg_private *priv)
{
	int ret = abox_tplg_get_int(priv, ABOX_TKN_GID);

	if (ret < 0)
		ret = ABOX_TPLG_GID_DEFAULT;

	return ret;
}

static int abox_tplg_get_min(struct snd_soc_tplg_private *priv)
{
	int ret = abox_tplg_get_int(priv, ABOX_TKN_MIN);

	return (ret < 0) ? 0 : ret;
}

static int abox_tplg_get_count(struct snd_soc_tplg_private *priv)
{
	int ret = abox_tplg_get_int(priv, ABOX_TKN_COUNT);

	return (ret < 1) ? 1 : ret;
}

static bool abox_tplg_is_volatile(struct snd_soc_tplg_private *priv)
{
	return abox_tplg_get_bool(priv, ABOX_TKN_VOLATILE);
}

static bool abox_tplg_synchronous(struct snd_soc_tplg_private *priv)
{
	return abox_tplg_get_bool(priv, ABOX_TKN_SYNCHRONOUS);
}

static unsigned int abox_tplg_get_address(struct snd_soc_tplg_private *priv)
{
	int ret = abox_tplg_get_int(priv, ABOX_TKN_ADDRESS);

	return (ret < 0 && ret > -MAX_ERRNO) ? 0 : ret;
}

static bool abox_tplg_is_weak(struct snd_soc_tplg_private *priv)
{
	return abox_tplg_get_bool(priv, ABOX_TKN_WEAK);
}

static bool abox_tplg_force_restore(struct snd_soc_tplg_private *priv)
{
	return abox_tplg_get_bool(priv, ABOX_TKN_FORCE_RESTORE);
}

static DECLARE_COMPLETION(report_control_completion);
static DECLARE_COMPLETION(update_control_completion);

static int abox_tplg_ipc_get(struct device *dev, int gid, int id)
{
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	unsigned long timeout;
	int ret;

	abox_dbg(dev, "%s(%#x, %d)\n", __func__, gid, id);

	reinit_completion(&report_control_completion);

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_REQUEST_COMPONENT_CONTROL;
	system_msg->param1 = gid;
	system_msg->param2 = id;
	ret = abox_tplg_request_ipc(&msg);
	if (ret < 0)
		return ret;

	timeout = wait_for_completion_timeout(&report_control_completion,
			abox_get_waiting_jiffies(true));
	if (timeout <= 0)
		return -ETIME;

	return 0;
}

static int abox_tplg_ipc_get_complete(int gid, int id, unsigned int *value)
{
	struct abox_tplg_kcontrol_data *kdata;
	int i;

	list_for_each_entry(kdata, &kcontrol_list, list) {
		if (kdata->gid == gid && kdata->id == id) {
			struct device *dev = kdata->cmpnt->dev;

			for (i = 0; i < kdata->count; i++) {
				abox_dbg(dev, "%s: %#x, %#x, %d\n", __func__,
						gid, id, value[i]);
				kdata->value[i] = value[i];
			}
			complete(&report_control_completion);
			return 0;
		}
	}

	return -EINVAL;
}

static int abox_tplg_ipc_put_bundle(struct device *dev, int gid, int id,
		unsigned int value, const char *bundle, bool sync)
{
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	unsigned long timeout;
	int ret;

	abox_dbg(dev, "%s(%#x, %d, %s, %d)\n", __func__, gid, id, bundle, sync);

	if (sync)
		reinit_completion(&update_control_completion);

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_UPDATE_COMPONENT_CONTROL;
	system_msg->param1 = gid;
	system_msg->param2 = id;
	system_msg->param3 = value;
	strscpy(system_msg->bundle.param_bundle, bundle,
			sizeof(system_msg->bundle.param_bundle));

	ret = abox_tplg_request_ipc(&msg);

	if (sync) {
		timeout = wait_for_completion_timeout(
				&update_control_completion,
				abox_get_waiting_jiffies(true));
		if (timeout <= 0)
			return -ETIME;
	}

	return ret;
}

static int abox_tplg_ipc_put(struct device *dev, int gid, int id,
		unsigned int *value, int count, bool sync)
{
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	unsigned long timeout;
	int i, ret;

	abox_dbg(dev, "%s(%#x, %d, %u, %d, %d)\n", __func__, gid, id,
			value[0], count, sync);

	if (sync)
		reinit_completion(&update_control_completion);

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_UPDATE_COMPONENT_CONTROL;
	system_msg->param1 = gid;
	system_msg->param2 = id;
	for (i = 0; i < count; i++)
		system_msg->bundle.param_s32[i] = value[i];

	ret = abox_tplg_request_ipc(&msg);

	if (sync) {
		timeout = wait_for_completion_timeout(
				&update_control_completion,
				abox_get_waiting_jiffies(true));
		if (timeout <= 0)
			return -ETIME;
	}

	return ret;
}

static int abox_tplg_ipc_put_complete(int gid, int id, unsigned int *value)
{
	struct abox_tplg_kcontrol_data *kdata;
	int i;

	list_for_each_entry(kdata, &kcontrol_list, list) {
		if (kdata->gid == gid && kdata->id == id) {
			struct device *dev = kdata->cmpnt->dev;

			for (i = 0; i < kdata->count; i++) {
				abox_dbg(dev, "%s: %#x, %#x, %d\n", __func__,
						gid, id, value[i]);
				kdata->value[i] = value[i];
			}
			complete(&update_control_completion);
			return 0;
		}
	}

	return -EINVAL;
}

static int abox_tplg_val_get(struct device *dev,
		struct abox_tplg_kcontrol_data *kdata)
{
	int i;

	abox_dbg(dev, "%s(%#x, %d)\n", __func__, kdata->gid, kdata->id);

	for (i = 0; i < kdata->count; i++) {
		kdata->value[i] = kdata->kaddr[i];
		abox_dbg(dev, "%d\n", kdata->value[i]);
	}

	return 0;
}

static void abox_tplg_val_set(struct device *dev,
		struct abox_tplg_kcontrol_data *kdata)
{
	int i;

	abox_dbg(dev, "%s(%#x, %d, %u)\n", __func__, kdata->gid, kdata->id,
			kdata->value[0]);

	for (i = 0; i < kdata->count; i++)
		kdata->kaddr[i] = kdata->value[i];
}

static int abox_tplg_val_put(struct device *dev,
		struct abox_tplg_kcontrol_data *kdata)
{
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	unsigned long timeout;
	int i, ret;

	abox_dbg(dev, "%s(%#x, %d, %u)\n", __func__, kdata->gid, kdata->id,
			kdata->value[0]);

	abox_tplg_val_set(dev, kdata);

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_UPDATE_COMPONENT_VALUE;
	system_msg->param1 = kdata->gid;
	system_msg->param2 = kdata->id;
	for (i = 0; i < kdata->count; i++)
		system_msg->bundle.param_s32[i] = kdata->value[i];

	ret = abox_tplg_request_ipc(&msg);

	if (kdata->synchronous) {
		timeout = wait_for_completion_timeout(
				&update_control_completion,
				abox_get_waiting_jiffies(true));
		if (timeout <= 0)
			return -ETIME;
	}

	return ret;
}

static bool abox_tplg_is_virtual_control(struct abox_tplg_kcontrol_data *kdata)
{
	return (kdata->hdr->ops.put == ABOX_TPLG_DAPM_CTL_ENUM_VIRT);
}

static inline int abox_tplg_kcontrol_get(struct device *dev,
		struct abox_tplg_kcontrol_data *kdata)
{
	if (abox_tplg_is_virtual_control(kdata))
		return 0;

	if (kdata->addr)
		return abox_tplg_val_get(dev, kdata);
	else
		return abox_tplg_ipc_get(dev, kdata->gid, kdata->id);
}

static inline int abox_tplg_kcontrol_put(struct device *dev,
		struct abox_tplg_kcontrol_data *kdata)
{
	if (abox_tplg_is_virtual_control(kdata))
		return 0;

	if (kdata->addr)
		return abox_tplg_val_put(dev, kdata);
	else
		return abox_tplg_ipc_put(dev, kdata->gid, kdata->id,
				kdata->value, kdata->count, kdata->synchronous);
}

static inline int abox_tplg_pipeline_put(struct device *dev,
		struct abox_tplg_kcontrol_data *kdata)
{
	return abox_tplg_ipc_put_bundle(dev, kdata->gid, kdata->id, kdata->value[0],
			kdata->pls.pl[kdata->value[0]].name, kdata->synchronous);
}

static inline int abox_tplg_kcontrol_restore(struct device *dev,
		struct abox_tplg_kcontrol_data *kdata)
{
	int ret = 0;

	if (abox_tplg_is_virtual_control(kdata))
		return 0;

	if (kdata->addr)
		abox_tplg_val_set(dev, kdata);
	else if (kdata->pls.count)
		ret = abox_tplg_ipc_put_bundle(dev, kdata->gid, kdata->id, kdata->value[0],
				kdata->pls.pl[kdata->value[0]].name, kdata->synchronous);
	else
		ret = abox_tplg_ipc_put(dev, kdata->gid, kdata->id,
				kdata->value, kdata->count, kdata->synchronous);

	return ret;
}

static inline int abox_tplg_widget_get(struct device *dev,
		struct abox_tplg_widget_data *wdata)
{
	return abox_tplg_ipc_get(dev, wdata->gid, wdata->id);
}

static inline int abox_tplg_widget_put(struct device *dev,
		struct abox_tplg_widget_data *wdata)
{
	return abox_tplg_ipc_put(dev, wdata->gid, wdata->id, &wdata->value, 1,
			false);
}

static int abox_tplg_mixer_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct device *dev = w->dapm->dev;
	struct abox_tplg_widget_data *wdata = w->dobj.private;

	abox_dbg(dev, "%s(%s, %d)\n", __func__, w->name, e);

	switch (e) {
	case SND_SOC_DAPM_PRE_PMU:
		wdata->value = 1;
		break;
	case SND_SOC_DAPM_POST_PMD:
		wdata->value = 0;
		break;
	default:
		return -EINVAL;
	}

	return abox_tplg_widget_put(dev, wdata);
}

static int abox_tplg_mux_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct device *dev = w->dapm->dev;

	abox_dbg(dev, "%s(%s, %d)\n", __func__, w->name, e);

	return 0;
}

static const struct snd_soc_tplg_widget_events abox_tplg_widget_ops[] = {
	{ABOX_EVENT_NONE, NULL},
	{ABOX_EVENT_MIXER, abox_tplg_mixer_event},
	{ABOX_EVENT_MUX, abox_tplg_mux_event},
};

static int abox_tplg_widget_load(struct snd_soc_component *cmpnt, int index,
		struct snd_soc_dapm_widget *w,
		struct snd_soc_tplg_dapm_widget *tplg_w)
{
	abox_dbg(cmpnt->dev, "%s(%d, %d, %s)\n", __func__,
			tplg_w->size, tplg_w->id, tplg_w->name);

	return snd_soc_tplg_widget_bind_event(w, abox_tplg_widget_ops,
			ARRAY_SIZE(abox_tplg_widget_ops),
			tplg_w->event_type);
}

static int abox_tplg_widget_ready(struct snd_soc_component *cmpnt, int index,
		struct snd_soc_dapm_widget *w,
		struct snd_soc_tplg_dapm_widget *tplg_w)
{
	struct device *dev = cmpnt->dev;
	struct abox_tplg_widget_data *wdata;
	struct snd_soc_dapm_route route;
	int id, gid, ret = 0;
	bool weak;

	abox_dbg(dev, "%s(%d, %d, %s)\n", __func__,
			tplg_w->size, tplg_w->id, tplg_w->name);

	id = abox_tplg_get_id(&tplg_w->priv);
	if (id < 0) {
		abox_err(dev, "%s: invalid widget id: %d\n", tplg_w->name, id);
		return id;
	}
	gid = abox_tplg_get_gid(&tplg_w->priv);
	if (gid < 0) {
		abox_err(dev, "%s: invalid widget gid: %d\n", tplg_w->name, gid);
		return gid;
	}
	weak = abox_tplg_is_weak(&tplg_w->priv);

	wdata = kzalloc(sizeof(*wdata), GFP_KERNEL);
	if (!wdata)
		return -ENOMEM;

	wdata->id = id;
	wdata->gid = gid;
	wdata->weak = weak;
	wdata->cmpnt = cmpnt;
	wdata->w = w;
	wdata->tplg_w = tplg_w;
	w->dobj.private = wdata;
	list_add_tail(&wdata->list, &widget_list);

	switch (tplg_w->id) {
	case SND_SOC_TPLG_DAPM_MUX:
		/* Add none route in here. */
		route.sink = tplg_w->name;
		route.control = "None";
		route.source = "None";
		route.connected = NULL;
		ret = snd_soc_dapm_add_routes(w->dapm, &route, 1);
		break;
	case SND_SOC_TPLG_DAPM_MIXER:
		if (id < MIN_VIRTUAL_WIDGET_ID)
			abox_tplg_register_dump(dev, gid, id, tplg_w->name);
		break;
	}

	return ret;
}

static int abox_tplg_widget_unload(struct snd_soc_component *cmpnt,
		struct snd_soc_dobj *dobj)
{
	struct abox_tplg_widget_data *wdata = dobj->private;

	list_del(&wdata->list);
	kfree(dobj->private);
	return 0;
}

static int abox_tplg_get_mux(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	int ret = 0;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	if (!pm_runtime_suspended(dev_abox) && kdata->is_volatile) {
		ret = abox_tplg_kcontrol_get(dev, kdata);
		if (ret < 0)
			return ret;
	}

	ucontrol->value.enumerated.item[0] = kdata->value[0];

	return ret;
}

static int abox_tplg_put_mux(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	unsigned int value = ucontrol->value.enumerated.item[0];
	int ret = 0;

	if (value >= e->items) {
		abox_err(dev, "%s: value=%d, items=%d\n",
				kcontrol->id.name, value, e->items);
		return -EINVAL;
	}

	abox_dbg(dev, "%s(%s, %s)\n", __func__, kcontrol->id.name,
			e->texts[value]);

	kdata->value[0] = value;
	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_kcontrol_put(dev, kdata);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static int abox_tplg_get_pipeline(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	int ret = 0;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	ucontrol->value.enumerated.item[0] = kdata->value[0];

	return ret;
}

static unsigned int abox_tplg_pipeline_item_value(struct abox_tplg_pipeline_item *item)
{
	struct abox_tplg_kcontrol_data *kdata;
	struct device *dev;
	struct snd_kcontrol *kc;
	struct soc_mixer_control *mc;
	struct soc_enum *se;
	int i;

	if (item->value != UNKNOWN_VALUE)
		return item->value;

	kdata = item->kdata;
	dev = kdata->cmpnt->dev;
	kc = abox_tplg_get_kcontrol(kdata);
	switch (kdata->hdr->ops.info) {
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		se = (struct soc_enum *)kc->private_value;
		for (i = 0; i < se->items; i++) {
			if (!strcmp(se->texts[i], item->text)) {
				item->value = snd_soc_enum_item_to_val(se, i);
				break;
			}
		}
		break;
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
	case SND_SOC_TPLG_DAPM_CTL_PIN:
		mc = (struct soc_mixer_control *)kc->private_value;
		if (kstrtoint(item->text, 0, &i) < 0)
			break;
		if (i < mc->min || i > mc->max)
			break;
		item->value = i;
		break;
	default:
		abox_warn(dev, "unsupported pipeline item type: %d\n",
				kdata->hdr->ops.info);
		break;
	}

	if (item->value == UNKNOWN_VALUE)
		abox_err(dev, "%s: invalid pipeline item text: %s\n",
				kdata->hdr->name, item->text);

	return item->value;
}

static int abox_tplg_put_pipeline_item(struct abox_tplg_pipeline_item *item, bool reset)
{
	struct abox_tplg_kcontrol_data *kdata = item->kdata;
	struct abox_data *data = dev_get_drvdata(dev_abox);
	struct snd_soc_component *cmpnt;
	struct device *dev;
	struct snd_kcontrol *kc;
	int ret = 0;
	struct snd_ctl_elem_value ucontrol = { 0, };

	if (!kdata)
		kdata = item->kdata = abox_tplg_find_kcontrol_data(item->name);

	if (!kdata)
		return -EINVAL;

	cmpnt = kdata->cmpnt;
	dev = cmpnt->dev;
	kdata->value[0] = reset ? 0 : abox_tplg_pipeline_item_value(item);

	abox_dbg(dev, "%s(%s, %d): %u\n", __func__, item->name, reset, kdata->value[0]);

	if (data->debug_mode)
		abox_info(dev, "kcontrol %s : %s\n", item->name, reset ? "None" : item->text);

	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_kcontrol_put(dev, item->kdata);
		if (ret < 0) {
			abox_err(dev, "pipeline item put fail: %s\n", item->name);
			return ret;
		}
	}

	kc = abox_tplg_get_kcontrol(kdata);
	switch (kdata->hdr->ops.info) {
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		ucontrol.value.enumerated.item[0] = kdata->value[0];
		ret = snd_soc_dapm_put_enum_double(kc, &ucontrol);
		break;
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
	case SND_SOC_TPLG_DAPM_CTL_PIN:
		ucontrol.value.integer.value[0] = kdata->value[0];
		ret = snd_soc_dapm_put_volsw(kc, &ucontrol);
		break;
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_CTL_VOLSW_SX:
		/* nothing to do */
		break;
	default:
		abox_warn(dev, "unknown pipeline item type: %s\b", item->name);
		break;
	}

	return ret;
}

static int abox_tplg_put_pipeline(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct abox_tplg_pipeline *pl;
	struct abox_tplg_pipeline_item *item;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	unsigned int value = ucontrol->value.enumerated.item[0];
	LIST_HEAD(list_reset);
	LIST_HEAD(list_apply);
	int ret = 0;

	if (value >= e->items) {
		abox_err(dev, "%s: value=%d, items=%d\n",
				kcontrol->id.name, value, e->items);
		return -EINVAL;
	}

	abox_dbg(dev, "%s(%s, %s)\n", __func__, kcontrol->id.name, e->texts[value]);
	abox_info(dev, "pipeline %s : %s\n", kcontrol->id.name, e->texts[value]);

	/* create working list */
	pl = &kdata->pls.pl[value];
	for (item = pl->items; item - pl->items < pl->count; item++)
		INIT_LIST_HEAD(&item->work);

	pl = &kdata->pls.pl[kdata->value[0]];
	for (item = pl->items; item - pl->items < pl->count; item++)
		list_add(&item->work, &list_reset);

	pl = &kdata->pls.pl[value];
	for (item = pl->items; item - pl->items < pl->count; item++)
		list_move_tail(&item->work, &list_apply);

	/* reset previous */
	list_for_each_entry(item, &list_reset, work) {
		abox_dbg(dev, "pipeline item reset: %s\n", item->name);
		ret = abox_tplg_put_pipeline_item(item, true);
		if (ret < 0)
			abox_err(dev, "pipeline item reset fail: %s %d\n",
					item->name, ret);
	}

	/* apply current */
	list_for_each_entry(item, &list_apply, work) {
		abox_dbg(dev, "pipeline item apply: %s\n", item->name);
		ret = abox_tplg_put_pipeline_item(item, false);
		if (ret < 0)
			abox_err(dev, "pipeline item apply fail: %s %d\n",
					item->name, ret);
	}

	kdata->value[0] = value;

	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_pipeline_put(dev, kdata);
		if (ret < 0)
			dev_warn(dev, "%s: failed to report pipeline change: %d\n",
					kcontrol->id.name, ret);
	}

	return ret;
}

static int abox_tplg_dapm_get_mux_virt(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	return snd_soc_dapm_get_enum_double(kcontrol, ucontrol);
}

static int abox_tplg_dapm_put_mux_virt(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value >= e->items) {
		abox_err(dev, "%s: value=%d, items=%d\n",
				kcontrol->id.name, value, e->items);
		return -EINVAL;
	}

	abox_dbg(dev, "%s(%s, %s)\n", __func__, kcontrol->id.name,
			e->texts[value]);

	kdata->value[0] = value;

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int abox_tplg_dapm_get_mux(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	int ret;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	if (!pm_runtime_suspended(dev_abox) && kdata->is_volatile) {
		ret = abox_tplg_kcontrol_get(dev, kdata);
		if (ret < 0)
			return ret;

		ucontrol->value.enumerated.item[0] = kdata->value[0];

		ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
		if (ret < 0)
			return ret;
	}

	return snd_soc_dapm_get_enum_double(kcontrol, ucontrol);
}

static int abox_tplg_dapm_put_mux(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = e->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	unsigned int value = ucontrol->value.enumerated.item[0];
	int ret;

	if (value >= e->items) {
		abox_err(dev, "%s: value=%d, items=%d\n",
				kcontrol->id.name, value, e->items);
		return -EINVAL;
	}

	abox_dbg(dev, "%s(%s, %s)\n", __func__, kcontrol->id.name,
			e->texts[value]);

	kdata->value[0] = value;
	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_kcontrol_put(dev, kdata);
		if (ret < 0)
			return ret;
	}

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int abox_tplg_info_mixer(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;

	snd_soc_info_volsw_sx(kcontrol, uinfo);
	/* Android's libtinyalsa uses min and max of uinfo as it is,
	 * not the number of levels.
	 */
	uinfo->value.integer.min = mc->min;
	uinfo->value.integer.max = mc->platform_max;
	uinfo->count = kdata->count;
	return 0;
}

static int abox_tplg_get_mixer_write_only(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	int i;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	for (i = 0; i < kdata->count; i++)
		ucontrol->value.integer.value[i] = 0;

	return 0;
}

static int abox_tplg_get_mixer(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	int i, ret;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_kcontrol_get(dev, kdata);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < kdata->count; i++)
		ucontrol->value.integer.value[i] = kdata->value[i];

	return 0;
}

static int abox_tplg_put_mixer(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	long *value = ucontrol->value.integer.value;
	int i, ret;

	abox_dbg(dev, "%s(%s, %ld)\n", __func__, kcontrol->id.name, value[0]);

	for (i = 0; i < kdata->count; i++) {
		if (value[i] < mc->min || value[i] > mc->max) {
			abox_err(dev, "%s: value[%d]=%d, min=%d, max=%d\n",
					kcontrol->id.name, i, value[i], mc->min,
					mc->max);
			return -EINVAL;
		}

		kdata->value[i] = (unsigned int)value[i];
	}

	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_kcontrol_put(dev, kdata);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int abox_tplg_dapm_get_mixer(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	int ret;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	if (!pm_runtime_suspended(dev_abox) && kdata->is_volatile) {
		ret = abox_tplg_kcontrol_get(dev, kdata);
		if (ret < 0)
			return ret;

		ucontrol->value.integer.value[0] = kdata->value[0];

		ret = snd_soc_dapm_put_volsw(kcontrol, ucontrol);
		if (ret < 0)
			return ret;
	}

	return snd_soc_dapm_get_volsw(kcontrol, ucontrol);
}

static int abox_tplg_dapm_put_mixer(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct device *dev = cmpnt->dev;
	unsigned int value = (unsigned int)ucontrol->value.integer.value[0];
	int ret;

	abox_dbg(dev, "%s(%s, %u)\n", __func__, kcontrol->id.name, value);

	if (value < mc->min || value > mc->max) {
		abox_err(dev, "%s: value=%d, min=%d, max=%d\n",
				kcontrol->id.name, value, mc->min, mc->max);
		return -EINVAL;
	}

	kdata->value[0] = value;
	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_kcontrol_put(dev, kdata);
		if (ret < 0)
			return ret;
	}

	return snd_soc_dapm_put_volsw(kcontrol, ucontrol);
}

static int abox_tplg_dapm_get_pin(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);
	struct device *dev = cmpnt->dev;
	const char *pin = kdata->hdr->name;
	int ret;

	abox_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	if (!pm_runtime_suspended(dev_abox) && kdata->is_volatile) {
		ret = abox_tplg_kcontrol_get(dev, kdata);
		if (ret < 0)
			return ret;

		if (kdata->value[0])
			snd_soc_dapm_enable_pin(dapm, pin);
		else
			snd_soc_dapm_disable_pin(dapm, pin);

		snd_soc_dapm_sync(dapm);
	}

	ucontrol->value.integer.value[0] =
			snd_soc_dapm_get_pin_status(dapm, pin);

	return 0;
}

static int abox_tplg_dapm_put_pin(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct abox_tplg_kcontrol_data *kdata = mc->dobj.private;
	struct snd_soc_component *cmpnt = kdata->cmpnt;
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);
	struct device *dev = cmpnt->dev;
	const char *pin = kdata->hdr->name;
	unsigned int value = (unsigned int)ucontrol->value.integer.value[0];
	int ret;

	abox_dbg(dev, "%s(%s, %u)\n", __func__, kcontrol->id.name, value);

	if (value < mc->min || value > mc->max) {
		abox_err(dev, "%s: value=%d, min=%d, max=%d\n",
				kcontrol->id.name, value, mc->min, mc->max);
		return -EINVAL;
	}

	kdata->value[0] = !!value;
	if (!pm_runtime_suspended(dev_abox)) {
		ret = abox_tplg_kcontrol_put(dev, kdata);
		if (ret < 0)
			return ret;
	}

	if (kdata->value[0])
		snd_soc_dapm_enable_pin(dapm, pin);
	else
		snd_soc_dapm_disable_pin(dapm, pin);

	return snd_soc_dapm_sync(dapm);
}

static const struct snd_soc_tplg_kcontrol_ops abox_tplg_kcontrol_ops[] = {
	{
		ABOX_TPLG_DAPM_CTL_VOLSW,
		abox_tplg_dapm_get_mixer,
		abox_tplg_dapm_put_mixer,
		snd_soc_info_volsw,
	}, {
		ABOX_TPLG_DAPM_CTL_ENUM_DOUBLE,
		abox_tplg_dapm_get_mux,
		abox_tplg_dapm_put_mux,
		snd_soc_info_enum_double,
	}, {
		ABOX_TPLG_DAPM_CTL_ENUM_VIRT,
		abox_tplg_dapm_get_mux_virt,
		abox_tplg_dapm_put_mux_virt,
		snd_soc_info_enum_double,
	}, {
		ABOX_TPLG_DAPM_CTL_ENUM_VALUE,
		abox_tplg_dapm_get_mux,
		abox_tplg_dapm_put_mux,
		snd_soc_info_enum_double,
	}, {
		ABOX_TPLG_DAPM_CTL_PIN,
		abox_tplg_dapm_get_pin,
		abox_tplg_dapm_put_pin,
		snd_soc_dapm_info_pin_switch,
	}, {
		ABOX_TPLG_CTL_VOLSW,
		abox_tplg_get_mixer,
		abox_tplg_put_mixer,
		abox_tplg_info_mixer,
	}, {
		ABOX_TPLG_CTL_VOLSW_WRITE_ONLY,
		abox_tplg_get_mixer_write_only,
		abox_tplg_put_mixer,
		abox_tplg_info_mixer,
	}, {
		ABOX_TPLG_CTL_ENUM_DOUBLE,
		abox_tplg_get_mux,
		abox_tplg_put_mux,
		snd_soc_info_enum_double,
	}, {
		ABOX_TPLG_CTL_ENUM_VALUE,
		abox_tplg_get_mux,
		abox_tplg_put_mux,
		snd_soc_info_enum_double,
	}, {
		SND_SOC_TPLG_CTL_VOLSW_SX,
		abox_tplg_get_mixer,
		abox_tplg_put_mixer,
		abox_tplg_info_mixer,
	}, {
		ABOX_TPLG_CTL_PIPELINE,
		abox_tplg_get_pipeline,
		abox_tplg_put_pipeline,
		snd_soc_info_enum_double,
	}
};

static int abox_tplg_control_copy_enum_values(
		struct snd_soc_dobj_control *control,
		struct snd_soc_tplg_vendor_string_elem *string, int items)
{
	int i;

	control->dvalues = kcalloc(items, sizeof(control->dvalues[0]),
			GFP_KERNEL);
	if (!control->dvalues)
		return -ENOMEM;

	for (i = 0; i < items; i++)
		control->dvalues[i] = string[i].token;

	return 0;
}

static int abox_tplg_control_copy_enum_texts(
		struct snd_soc_dobj_control *control,
		struct snd_soc_tplg_vendor_string_elem *string, int items)
{
	int i;

	control->dtexts = kcalloc(items, sizeof(control->dtexts[0]),
			GFP_KERNEL);
	if (!control->dtexts)
		return -ENOMEM;

	for (i = 0; i < items; i++) {
		if (strnlen(string[i].string, SNDRV_CTL_ELEM_ID_NAME_MAXLEN) ==
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN)
			return -EINVAL;

		control->dtexts[i] = kstrdup(string[i].string, GFP_KERNEL);
	}

	return 0;
}

static int abox_tplg_control_load_enum_items(struct snd_soc_component *cmpnt,
		struct snd_soc_tplg_enum_control *tplg_ec,
		struct soc_enum *se)
{
	struct device *dev = cmpnt->dev;
	struct snd_soc_tplg_private *priv = &tplg_ec->priv;
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_dobj_control *control = &se->dobj.control;
	int i, sz, ret = -EINVAL;

	abox_dbg(dev, "%s(%s, %d)\n", __func__, tplg_ec->hdr.name, priv->size);

	for (sz = 0; sz < priv->size; sz += array->size) {
		array = (struct snd_soc_tplg_vendor_array *)(priv->data + sz);

		if (array->type != SND_SOC_TPLG_TUPLE_TYPE_STRING)
			continue;

		switch (tplg_ec->hdr.ops.info) {
		case SND_SOC_TPLG_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
			ret = abox_tplg_control_copy_enum_values(control,
					array->string, array->num_elems);
			if (ret < 0) {
				abox_err(dev, "invalid enum values: %d\n", ret);
				break;
			}
			/* fall through to create texts */
		case SND_SOC_TPLG_CTL_ENUM:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
			ret = abox_tplg_control_copy_enum_texts(control,
					array->string, array->num_elems);
			if (ret < 0) {
				abox_err(dev, "invalid enum texts: %d\n", ret);
				break;
			}
			se->texts = (const char * const *)control->dtexts;
			se->items = array->num_elems;
			break;
		default:
			break;
		}

		if (ret < 0) {
			kfree(control->dvalues);
			if (control->dtexts) {
				for (i = 0; i < array->num_elems; i++)
					kfree(control->dtexts[i]);
			}
			kfree(control->dtexts);
		}
		break;
	}

	return ret;
}

static int abox_tplg_control_load_pipeline_item_id(struct device *dev,
		struct soc_enum *se, struct snd_soc_tplg_vendor_array *array,
		struct snd_soc_tplg_vendor_string_elem *elem,
		struct abox_tplg_pipelines *pls, int pl_id)
{
	const char *enum_text;
	struct abox_tplg_pipeline *pl;
	int ret;

	if (pl_id >= pls->count)
		return -EINVAL;

	enum_text = se->dobj.control.dtexts[pl_id];
	ret = strncmp(enum_text, elem->string, sizeof(elem->string));
	if (ret) {
		abox_err(dev, "%s: unmatched enum text: %s != %s\n",
				se->dobj.control.kcontrol->id.name,
				enum_text, elem->string);
		BUG();
	}

	pl = &pls->pl[pl_id];
	pl->id = pl_id;
	abox_dbg(dev, "pipeline id=%u\n", pl->id);
	pl->tplg_va = array;
	pl->count = 4;
	pl->name = elem->string;
	pl->items = devm_kcalloc(dev, pl->count, sizeof(*pl->items), GFP_KERNEL);
	if (!pl->items)
		return -ENOMEM;

	return 0;
}

static int abox_tplg_control_load_pipeline_item_value(struct device *dev,
		struct snd_soc_tplg_vendor_string_elem *elem,
		struct abox_tplg_pipeline *pl, int pl_count)
{
	struct abox_tplg_pipeline_item *item;
	char buf[sizeof(elem->string)];
	char *name, *value;

	if (elem->token <= ABOX_TKN_PIPELINE)
		return -EINVAL;

	while (pl->count <= pl_count) {
		void *buf_new;

		pl->count = pl_count + 4;
		buf_new = devm_krealloc(dev, pl->items, sizeof(*pl->items) * pl->count, GFP_KERNEL | __GFP_ZERO);
		if (IS_ERR_OR_NULL(buf_new))
			return -ENOMEM;
		pl->items = buf_new;
	}

	strscpy(buf, elem->string, sizeof(buf));
	value = buf;
	name = strsep(&value, ",");
	if (!value || strlen(value) == 0)
		return -EINVAL;
	name = strim(name);
	value = strim(value);
	item = &pl->items[pl_count];
	strscpy(item->name, name, sizeof(item->name));
	strscpy(item->text, value, sizeof(item->text));
	abox_dbg(dev, "pipeline name=%s value=%s\n", item->name, item->text);
	item->value = UNKNOWN_VALUE;

	return 0;
}

static int abox_tplg_control_load_pipeline_items(
		struct snd_soc_component *cmpnt,
		struct snd_soc_tplg_enum_control *tplg_ec,
		struct soc_enum *se,
		struct abox_tplg_kcontrol_data *kdata)
{
	struct device *dev = cmpnt->dev;
	struct snd_soc_tplg_private *priv = &tplg_ec->priv;
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_string_elem *elem;
	struct abox_tplg_pipelines *pls = &kdata->pls;
	struct abox_tplg_pipeline *pl;
	int sz, pl_id, pl_count, ret = 0;

	abox_dbg(dev, "%s(%s, %d)\n", __func__, tplg_ec->hdr.name, priv->size);

	pls->count = se->items;
	pls->pl = devm_kcalloc(dev, pls->count, sizeof(*pls->pl), GFP_KERNEL);
	if (!pls->pl)
		return -ENOMEM;

	pl_id = -1;
	pl_count = 0;
	for (sz = 0; sz < priv->size; sz += array->size) {
		array = (struct snd_soc_tplg_vendor_array *)(priv->data + sz);

		if (array->type != SND_SOC_TPLG_TUPLE_TYPE_STRING)
			continue;

		if (array->num_elems < 1)
			continue;

		for (elem = array->string; elem - array->string < array->num_elems; elem++) {
			if (elem->token < ABOX_TKN_PIPELINE)
				continue;

			switch (elem->token) {
			case ABOX_TKN_PIPELINE:
				ret = abox_tplg_control_load_pipeline_item_id(
						dev, se, array, elem, pls, ++pl_id);
				if (ret < 0)
					break;

				pl_count = 0;
			default:
				if (pl_id < 0)
					break;

				pl = &pls->pl[pl_id];
				ret = abox_tplg_control_load_pipeline_item_value(
						dev, elem, pl, pl_count);
				if (ret < 0)
					break;

				pl_count++;
			}
		}

		if (pl_id < 0)
			continue;

		pl = &pls->pl[pl_id];
		pl->count = pl_count;
		abox_dbg(dev, "pipeline %s count=%u\n", pl->name, pl->count);
	}

	return ret;
}

static int abox_tplg_control_load_enum(struct snd_soc_component *cmpnt,
		struct snd_kcontrol_new *kctl,
		struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct device *dev = cmpnt->dev;
	struct abox_tplg_kcontrol_data *kdata;
	struct snd_soc_tplg_enum_control *tplg_ec;
	struct soc_enum *se = (struct soc_enum *)kctl->private_value;
	int id, gid, ret;

	tplg_ec = container_of(hdr, struct snd_soc_tplg_enum_control, hdr);
	id = abox_tplg_get_id(&tplg_ec->priv);
	if (id < 0) {
		abox_err(dev, "%s: invalid enum id: %d\n", hdr->name, id);
		return id;
	}
	gid = abox_tplg_get_gid(&tplg_ec->priv);
	if (gid < 0) {
		abox_err(dev, "%s: invalid enum gid: %d\n", hdr->name, gid);
		return gid;
	}

	ret = abox_tplg_control_load_enum_items(cmpnt, tplg_ec, se);
	if (ret < 0) {
		abox_err(dev, "%s: invalid enum items: %d\n", hdr->name, ret);
		return ret;
	}

	if (se->reg < SND_SOC_NOPM) {
		se->reg = SND_SOC_NOPM;
		se->shift_l = se->shift_r = 0;
		se->mask = ~0U;
	}

	kdata = kzalloc(sizeof(*kdata), GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;

	kdata->gid = gid;
	kdata->id = id;
	kdata->count = abox_tplg_get_count(&tplg_ec->priv);
	kdata->is_volatile = abox_tplg_is_volatile(&tplg_ec->priv);
	kdata->synchronous = abox_tplg_synchronous(&tplg_ec->priv);
	kdata->force_restore = abox_tplg_force_restore(&tplg_ec->priv);
	kdata->cmpnt = cmpnt;
	kdata->kcontrol_new = kctl;
	kdata->tplg_ec = tplg_ec;
	kdata->dobj = &se->dobj;
	se->dobj.private = kdata;
	mutex_lock(&kcontrol_mutex);
	list_add_tail(&kdata->list, &kcontrol_list);
	mutex_unlock(&kcontrol_mutex);

	return 0;
}

static int abox_tplg_control_load_mixer(struct snd_soc_component *cmpnt,
		struct snd_kcontrol_new *kctl,
		struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev_abox);
	struct abox_tplg_kcontrol_data *kdata;
	struct snd_soc_tplg_mixer_control *tplg_mc;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kctl->private_value;
	int id, gid;

	tplg_mc = container_of(hdr, struct snd_soc_tplg_mixer_control, hdr);
	id = abox_tplg_get_id(&tplg_mc->priv);
	if (id < 0) {
		abox_err(dev, "%s: invalid enum id: %d\n", hdr->name, id);
		return id;
	}
	gid = abox_tplg_get_gid(&tplg_mc->priv);
	if (gid < 0) {
		abox_err(dev, "%s: invalid enum gid: %d\n", hdr->name, gid);
		return gid;
	}

	if (mc->reg < SND_SOC_NOPM) {
		mc->reg = mc->rreg = SND_SOC_NOPM;
		mc->shift = mc->rshift = 0;
	}

	kdata = kzalloc(sizeof(*kdata), GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;

	kdata->gid = gid;
	kdata->id = id;
	kdata->count = abox_tplg_get_count(&tplg_mc->priv);
	kdata->is_volatile = abox_tplg_is_volatile(&tplg_mc->priv);
	kdata->synchronous = abox_tplg_synchronous(&tplg_mc->priv);
	kdata->force_restore = abox_tplg_force_restore(&tplg_mc->priv);
	kdata->addr = abox_tplg_get_address(&tplg_mc->priv);
	if (kdata->addr)
		kdata->kaddr = abox_addr_to_kernel_addr(data, kdata->addr);
	kdata->cmpnt = cmpnt;
	kdata->kcontrol_new = kctl;
	kdata->tplg_mc = tplg_mc;
	kdata->dobj = &mc->dobj;
	mc->dobj.private = kdata;
	mutex_lock(&kcontrol_mutex);
	list_add_tail(&kdata->list, &kcontrol_list);
	mutex_unlock(&kcontrol_mutex);

	return 0;
}

static int abox_tplg_control_load_sx(struct snd_soc_component *cmpnt,
		struct snd_kcontrol_new *kctl,
		struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct snd_soc_tplg_mixer_control *tplg_mc;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kctl->private_value;

	tplg_mc = container_of(hdr, struct snd_soc_tplg_mixer_control, hdr);
	/* Current ALSA topology doesn't process soc_mixer_control->min. */
	mc->min = abox_tplg_get_min(&tplg_mc->priv);
	return abox_tplg_control_load_mixer(cmpnt, kctl, hdr);
}

static int abox_tplg_control_load_pipeline(struct snd_soc_component *cmpnt,
		struct snd_kcontrol_new *kctl,
		struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct device *dev = cmpnt->dev;
	struct abox_tplg_kcontrol_data *kdata;
	struct snd_soc_tplg_enum_control *tplg_ec;
	struct soc_enum *se = (struct soc_enum *)kctl->private_value;
	int id, gid, ret;

	tplg_ec = container_of(hdr, struct snd_soc_tplg_enum_control, hdr);
	id = abox_tplg_get_id(&tplg_ec->priv);
	if (id < 0) {
		abox_err(dev, "%s: invalid enum id: %d\n", hdr->name, id);
		return id;
	}
	gid = abox_tplg_get_gid(&tplg_ec->priv);
	if (gid < 0) {
		abox_err(dev, "%s: invalid enum gid: %d\n", hdr->name, gid);
		return gid;
	}

	ret = abox_tplg_control_load_enum_items(cmpnt, tplg_ec, se);
	if (ret < 0) {
		abox_err(dev, "%s: invalid enum items: %d\n", hdr->name, ret);
		return ret;
	}

	if (se->reg < SND_SOC_NOPM) {
		se->reg = SND_SOC_NOPM;
		se->shift_l = se->shift_r = 0;
		se->mask = ~0U;
	}

	kdata = kzalloc(sizeof(*kdata), GFP_KERNEL);
	if (!kdata)
		return -ENOMEM;

	kdata->gid = gid;
	kdata->id = id;
	kdata->count = abox_tplg_get_count(&tplg_ec->priv);
	kdata->is_volatile = abox_tplg_is_volatile(&tplg_ec->priv);
	kdata->synchronous = abox_tplg_synchronous(&tplg_ec->priv);
	kdata->force_restore = abox_tplg_force_restore(&tplg_ec->priv);
	kdata->cmpnt = cmpnt;
	kdata->kcontrol_new = kctl;
	kdata->tplg_ec = tplg_ec;
	kdata->dobj = &se->dobj;
	se->dobj.private = kdata;
	ret = abox_tplg_control_load_pipeline_items(cmpnt, tplg_ec, se, kdata);
	if (ret < 0) {
		abox_err(dev, "%s: invalid piepline items: %d\n", hdr->name,
				ret);
		return ret;
	}
	list_add_tail(&kdata->list, &kcontrol_list);

	return 0;
}

int abox_tplg_control_load(struct snd_soc_component *cmpnt, int index,
		struct snd_kcontrol_new *kctl,
		struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct device *dev = cmpnt->dev;
	int ret = 0;

	abox_dbg(cmpnt->dev, "%s(%d, %d, %s)\n", __func__,
			hdr->size, hdr->type, hdr->name);

	switch (hdr->ops.info) {
	case SND_SOC_TPLG_CTL_ENUM:
	case SND_SOC_TPLG_CTL_ENUM_VALUE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
	case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		if (!(kctl->access & SNDRV_CTL_ELEM_ACCESS_READWRITE))
			break;

		switch (hdr->ops.get) {
		case ABOX_TPLG_CTL_PIPELINE:
			ret = abox_tplg_control_load_pipeline(cmpnt, kctl, hdr);
			break;
		default:
			ret = abox_tplg_control_load_enum(cmpnt, kctl, hdr);
			break;
		}
		break;
	case SND_SOC_TPLG_CTL_VOLSW:
	case SND_SOC_TPLG_DAPM_CTL_VOLSW:
	case SND_SOC_TPLG_DAPM_CTL_PIN:
		if (kctl->access & SNDRV_CTL_ELEM_ACCESS_READWRITE)
			ret = abox_tplg_control_load_mixer(cmpnt, kctl, hdr);
		break;
	case SND_SOC_TPLG_CTL_VOLSW_SX:
		if (kctl->access & SNDRV_CTL_ELEM_ACCESS_READWRITE)
			ret = abox_tplg_control_load_sx(cmpnt, kctl, hdr);
		break;
	default:
		abox_warn(dev, "unknown control %s:%d\n", hdr->name,
				hdr->ops.info);
		break;
	}

	return ret;
}

int abox_tplg_control_unload(struct snd_soc_component *cmpnt,
		struct snd_soc_dobj *dobj)
{
	struct abox_tplg_kcontrol_data *kdata = dobj->private;

	list_del(&kdata->list);
	kfree(dobj->private);
	return 0;
}

static int abox_tplg_dai_load(struct snd_soc_component *cmpnt, int index,
		struct snd_soc_dai_driver *dai_drv,
		struct snd_soc_tplg_pcm *pcm, struct snd_soc_dai *dai)
{
	struct device *dev = cmpnt->dev;
	struct device *dev_platform;
	struct abox_tplg_dai_data *data;
	struct snd_pcm_hardware *hardware, playback = {0, }, capture = {0, };
	struct snd_soc_tplg_stream_caps *caps;

	abox_dbg(dev, "%s(%s)\n", __func__, dai_drv->name);

	if (pcm->playback) {
		hardware = &playback;
		caps = &pcm->caps[SNDRV_PCM_STREAM_PLAYBACK];
		hardware->formats = caps->formats;
		hardware->rates = caps->rates;
		hardware->rate_min = caps->rate_min;
		hardware->rate_max = caps->rate_max;
		hardware->channels_min = caps->channels_min;
		hardware->channels_max = caps->channels_max;
		hardware->buffer_bytes_max = caps->buffer_size_max;
		hardware->period_bytes_min = caps->period_size_min;
		hardware->period_bytes_max = caps->period_size_max;
		hardware->periods_min = caps->periods_min;
		hardware->periods_max = caps->periods_max;
	}

	if (pcm->capture) {
		hardware = &capture;
		caps = &pcm->caps[SNDRV_PCM_STREAM_CAPTURE];
		hardware->formats = caps->formats;
		hardware->rates = caps->rates;
		hardware->rate_min = caps->rate_min;
		hardware->rate_max = caps->rate_max;
		hardware->channels_min = caps->channels_min;
		hardware->channels_max = caps->channels_max;
		hardware->buffer_bytes_max = caps->buffer_size_max;
		hardware->period_bytes_min = caps->period_size_min;
		hardware->period_bytes_max = caps->period_size_max;
		hardware->periods_min = caps->periods_min;
		hardware->periods_max = caps->periods_max;
	}

	dev_platform = abox_vdma_register_component(dev,
			dai_drv->id, dai_drv->name, &playback, &capture);
	if (IS_ERR(dev_platform))
		dev_platform = NULL;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->id = dai_drv->id;
	data->dai_drv = dai_drv;
	data->dev_platform = dev_platform;
	dai_drv->dobj.private = data;
	list_add_tail(&data->list, &dai_list);

	return 0;
}

static int abox_tplg_dai_unload(struct snd_soc_component *cmpnt,
		struct snd_soc_dobj *dobj)
{
	struct abox_tplg_dai_data *data = dobj->private;

	list_del(&data->list);
	kfree(dobj->private);
	return 0;
}

static int abox_tplg_link_load(struct snd_soc_component *cmpnt, int index,
		struct snd_soc_dai_link *link,
		struct snd_soc_tplg_link_config *cfg)
{
	struct device *dev = cmpnt->dev;
	struct snd_soc_dai *dai;

	abox_dbg(dev, "%s(%s)\n", __func__, link->stream_name);

	if (cfg) {
		struct snd_soc_tplg_private *priv = &cfg->priv;
		unsigned int rate, width, channels, period_size, periods;
		bool packed;

		rate = abox_tplg_get_int(priv, ABOX_TKN_RATE);
		width = abox_tplg_get_int(priv, ABOX_TKN_WIDTH);
		channels = abox_tplg_get_int(priv, ABOX_TKN_CHANNELS);
		period_size = abox_tplg_get_int(priv, ABOX_TKN_PERIOD_SIZE);
		periods = abox_tplg_get_int(priv, ABOX_TKN_PERIODS);
		packed = abox_tplg_get_bool(priv, ABOX_TKN_PACKED);

		dai = snd_soc_find_dai(link->cpus);
		if (dai)
			abox_dma_hw_params_set(dai->dev, rate, width, channels,
					period_size, periods, packed);
		else
			abox_err(dev, "%s: can't find dai\n", link->name);
	}

	list_for_each_entry(dai, &cmpnt->dai_list, list) {
		if (strcmp(dai->name, link->cpus->dai_name) == 0) {
			struct abox_tplg_dai_data *data;

			data = dai->driver->dobj.private;
			if (!data || !data->dev_platform)
				continue;
			link->platforms->name = dev_name(data->dev_platform);
			link->ignore_suspend = 1;
			break;
		}
	}

	return 0;
}

static int abox_tplg_link_unload(struct snd_soc_component *cmpnt,
		struct snd_soc_dobj *dobj)
{
	/* nothing to do */
	return 0;
}

static void abox_tplg_route_mux(struct snd_soc_component *cmpnt,
		struct abox_tplg_widget_data *wdata)
{
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);
	struct snd_soc_tplg_dapm_widget *tplg_w = wdata->tplg_w;
	struct snd_soc_dapm_widget *w = wdata->w;
	const struct snd_kcontrol_new *kcontrol_new = w->kcontrol_news;
	struct abox_tplg_widget_data *wdata_src;
	struct soc_enum *e;
	struct snd_soc_dapm_route route = {0, };
	int i;

	abox_dbg(cmpnt->dev, "%s(%s)\n", __func__, tplg_w->name);
	e = (struct soc_enum *)kcontrol_new->private_value;

	/* Auto-generating routes in driver is more brilliant
	 * than typing all routes in the topology file.
	 */
	for (i = 0; i < e->items; i++) {
		list_for_each_entry(wdata_src, &widget_list, list) {
			if (strcmp(wdata_src->tplg_w->name, e->texts[i]))
				continue;

			route.sink = tplg_w->name;
			route.control = e->texts[i];
			route.source = e->texts[i];
			snd_soc_dapm_add_routes(dapm, &route, 1);
			if (wdata->weak) {
				route.control = NULL;
				snd_soc_dapm_weak_routes(dapm, &route, 1);
			}
			break;
		}
	}
}

static void abox_tplg_complete(struct snd_soc_component *cmpnt)
{
	abox_dbg(cmpnt->dev, "%s\n", __func__);
}

/* Dummy widget for connect to none. */
static const struct snd_soc_dapm_widget abox_tplg_widgets[] = {
	SND_SOC_DAPM_PGA("None", SND_SOC_NOPM, 0, 0, NULL, 0),
};

static int abox_tplg_bin_load(struct device *dev,
		struct snd_soc_tplg_private *priv)
{
	struct abox_data *data = dev_get_drvdata(dev_abox);
	const char *name;
	bool changeable;
	int idx, area, offset, i;
	int ret = 0;

	for (i = 0; ret >= 0; i++) {
		idx = abox_tplg_get_int_at(priv, ABOX_BIN_IDX, i);
		if (idx < 0)
			break;

		name = abox_tplg_get_string_at(priv, ABOX_BIN_NAME, i);
		if (IS_ERR_OR_NULL(name))
			break;

		area = abox_tplg_get_int_at(priv, ABOX_BIN_AREA, i);
		if (area < 0)
			break;

		offset = abox_tplg_get_int_at(priv, ABOX_BIN_OFFSET, i);
		if (offset < 0)
			break;

		changeable = abox_tplg_get_bool_at(priv, ABOX_BIN_CHANGEABLE, i);

		ret = abox_add_extra_firmware(dev, data, idx, name, area,
				offset, changeable);
	}

	return ret;
}

static int abox_tplg_manifest(struct snd_soc_component *cmpnt, int index,
		struct snd_soc_tplg_manifest *manifest)
{
	abox_dbg(cmpnt->dev, "%s\n", __func__);

	return abox_tplg_bin_load(cmpnt->dev, &manifest->priv);
}

static struct snd_soc_tplg_ops abox_tplg_ops  = {
	.widget_load	= abox_tplg_widget_load,
	.widget_ready	= abox_tplg_widget_ready,
	.widget_unload	= abox_tplg_widget_unload,
	.control_load	= abox_tplg_control_load,
	.control_unload	= abox_tplg_control_unload,
	.dai_load	= abox_tplg_dai_load,
	.dai_unload	= abox_tplg_dai_unload,
	.link_load	= abox_tplg_link_load,
	.link_unload	= abox_tplg_link_unload,
	.complete	= abox_tplg_complete,
	.manifest	= abox_tplg_manifest,
	.io_ops		= abox_tplg_kcontrol_ops,
	.io_ops_count	= ARRAY_SIZE(abox_tplg_kcontrol_ops),
};

static irqreturn_t abox_tplg_ipc_handler(int ipc_id, void *dev_id,
		ABOX_IPC_MSG *msg)
{
	struct IPC_SYSTEM_MSG *system = &msg->msg.system;
	irqreturn_t ret = IRQ_NONE;

	switch (system->msgtype) {
	case ABOX_REPORT_COMPONENT_CONTROL:
		if (abox_tplg_ipc_get_complete(system->param1, system->param2,
				(unsigned int *)system->bundle.param_s32) >= 0)
			ret = IRQ_HANDLED;
		break;
	case ABOX_UPDATED_COMPONENT_CONTROL:
		if (abox_tplg_ipc_put_complete(system->param1, system->param2,
				(unsigned int *)system->bundle.param_s32) >= 0)
			ret = IRQ_HANDLED;
		break;
	default:
		break;
	}

	return ret;
}

int abox_tplg_restore(struct device *dev)
{
	struct abox_tplg_kcontrol_data *kdata;
	int i;

	abox_dbg(dev, "%s\n", __func__);

	mutex_lock(&kcontrol_mutex);
	list_for_each_entry(kdata, &kcontrol_list, list) {
		switch (kdata->hdr->ops.info) {
		case SND_SOC_TPLG_CTL_ENUM:
		case SND_SOC_TPLG_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_CTL_VOLSW:
		case SND_SOC_TPLG_CTL_VOLSW_SX:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_DOUBLE:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VIRT:
		case SND_SOC_TPLG_DAPM_CTL_ENUM_VALUE:
		case SND_SOC_TPLG_DAPM_CTL_VOLSW:
			/* Restore non-zero value only, because
			 * value in firmware is already zero after reset.
			 */
			for (i = 0; i < kdata->count; i++) {
				if (kdata->value[i] || kdata->force_restore) {
					abox_tplg_kcontrol_restore(dev, kdata);
					break;
				}
			}
			break;
		default:
			abox_warn(dev, "unknown control %s:%d\n",
				kdata->hdr->name, kdata->hdr->ops.info);
			break;
		}
	}
	mutex_unlock(&kcontrol_mutex);

	return 0;
}

int abox_tplg_probe(struct snd_soc_component *cmpnt)
{
	static const int retry = 80;
	struct device *dev = cmpnt->dev;
	struct abox_tplg_firmware *tplg_fw;
	struct abox_tplg_widget_data *wdata;
	int i, ret;

	abox_dbg(dev, "%s\n", __func__);

	ret = snd_soc_dapm_new_controls(snd_soc_component_get_dapm(cmpnt),
			abox_tplg_widgets, ARRAY_SIZE(abox_tplg_widgets));
	if (ret < 0)
		goto err_firmware;

	for (tplg_fw = abox_tplg_fw; tplg_fw - abox_tplg_fw < ARRAY_SIZE(abox_tplg_fw); tplg_fw++) {
		if (tplg_fw->fw) {
			release_firmware(tplg_fw->fw);
			tplg_fw->fw = NULL;
		}

		ret = firmware_request_nowarn(&tplg_fw->fw, tplg_fw->fw_name, dev);
		if (ret < 0 && tplg_fw->optional) {
			abox_info(dev, "not loaded %s\n", tplg_fw->fw_name);
			continue;
		}

		for (i = retry; ret && i; i--) {
			msleep(1000);
			ret = firmware_request_nowarn(&tplg_fw->fw, tplg_fw->fw_name, dev);
		}
		if (ret < 0) {
			ret = -ENODEV;
			goto err_firmware;
		}

		abox_info(dev, "loaded %s\n", tplg_fw->fw_name);

		ret = snd_soc_tplg_component_load(cmpnt, &abox_tplg_ops, tplg_fw->fw, 0);
		if (ret < 0)
			goto err_load;
	}

	list_for_each_entry(wdata, &widget_list, list) {
		if (wdata->tplg_w->id == SND_SOC_TPLG_DAPM_MUX)
			abox_tplg_route_mux(cmpnt, wdata);
	}

	ret = abox_ipc_register_handler(dev, IPC_SYSTEM, abox_tplg_ipc_handler, NULL);
	if (ret < 0)
		goto err_ipc;

	return ret;

err_ipc:
	snd_soc_tplg_component_remove(cmpnt, 0);
err_load:
	for (tplg_fw = abox_tplg_fw; tplg_fw - abox_tplg_fw < ARRAY_SIZE(abox_tplg_fw); tplg_fw++) {
		release_firmware(tplg_fw->fw);
		tplg_fw->fw = NULL;
	}
err_firmware:
	return ret;
}

void abox_tplg_remove(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;

	abox_dbg(dev, "%s\n", __func__);
}

static const struct snd_soc_component_driver abox_tplg = {
	.probe		= abox_tplg_probe,
	.remove		= abox_tplg_remove,
	.probe_order	= SND_SOC_COMP_ORDER_LAST,
};

static int samsung_abox_tplg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_abox = dev->parent;

	return snd_soc_register_component(dev, &abox_tplg, NULL, 0);
}

static int samsung_abox_tplg_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snd_soc_component *cmpnt;
	struct abox_tplg_firmware *tplg_fw;

	cmpnt = snd_soc_lookup_component(dev, NULL);
	if (cmpnt)
		snd_soc_tplg_component_remove(cmpnt, 0);
	snd_soc_unregister_component(dev);
	for (tplg_fw = abox_tplg_fw; tplg_fw - abox_tplg_fw < ARRAY_SIZE(abox_tplg_fw); tplg_fw++) {
		if (!tplg_fw->fw)
			continue;

		release_firmware(tplg_fw->fw);
		tplg_fw->fw = NULL;
	}
	dev_abox = NULL;

	return 0;
}

static const struct of_device_id samsung_abox_tplg_match[] = {
	{
		.compatible = "samsung,abox-tplg",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_tplg_match);

struct platform_driver samsung_abox_tplg_driver = {
	.probe = samsung_abox_tplg_probe,
	.remove = samsung_abox_tplg_remove,
	.driver = {
		.name = "abox-tplg",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_tplg_match),
	},
};
