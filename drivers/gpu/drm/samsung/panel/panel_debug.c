// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/time64.h>
#include "panel_drv.h"
#include "panel_firmware.h"
#include "panel_debug.h"
#include "panel_property.h"

#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
#include "panel_freq_hop.h"
#endif
#include "panel_obj.h"
#if defined(CONFIG_USDM_PANEL_FREQ_HOP) || defined(CONFIG_USDM_ADAPTIVE_MIPI)
#include "dev_ril_header.h"
#endif
#include <linux/sec_panel_notifier_v2.h>
#if defined(CONFIG_USDM_PANEL_JSON)
#include "ezop/json_writer.h"
#include "ezop/panel_json.h"
#endif

const char *panel_debugfs_file_name[MAX_PANEL_DEBUGFS_FILE] = {
	[PANEL_DEBUGFS_FILE_LOG] = "log",
	[PANEL_DEBUGFS_FILE_CMD_LOG] = "cmd_log",
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	[PANEL_DEBUGFS_FILE_FREQ_HOP] = "freq_hop",
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	[PANEL_DEBUGFS_FILE_PANEL_EVENT] = "panel_event",
#endif
#if defined(CONFIG_USDM_PANEL_JSON)
	[PANEL_DEBUGFS_FILE_EZOP] = "ezop",
	[PANEL_DEBUGFS_FILE_FIRMWARE] = "firmware",
#endif
#if defined(CONFIG_USDM_ADAPTIVE_MIPI)
	[PANEL_DEBUGFS_FILE_ADAPTIVE_MIPI] = "adaptive_mipi",
#endif
};

const char *panel_debugfs_dir_name[MAX_PANEL_DEBUGFS_DIR] = {
	[PANEL_DEBUGFS_DIR_SEQUENCE] = "sequence",
	[PANEL_DEBUGFS_DIR_RESOURCE] = "resource",
	[PANEL_DEBUGFS_DIR_PROPERTY] = "property",
};

static int string_to_debugfs_file_id(const char *str)
{
	int i;

	if (!str)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(panel_debugfs_file_name); i++) {
		if (!panel_debugfs_file_name[i])
			continue;

		if (!strcmp(panel_debugfs_file_name[i], str))
			return i;
	}

	return -EINVAL;
}

static int string_to_debugfs_dir_id(const char *str)
{
	int i;

	if (!str)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(panel_debugfs_dir_name); i++) {
		if (!panel_debugfs_dir_name[i])
			continue;

		if (!strcmp(panel_debugfs_dir_name[i], str))
			return i;
	}

	return -EINVAL;
}

static int panel_debug_log_show(struct seq_file *s)
{
	seq_printf(s, "%d\n", panel_log_level);

	return 0;
}

static int panel_debug_cmd_log_show(struct seq_file *s)
{
	seq_printf(s, "%d\n", panel_cmd_log);

	return 0;
}

static int panel_debug_sequence_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	char *buf = kzalloc(SZ_1K, GFP_KERNEL);

	if (!buf)
		return 0;

	snprintf_sequence(buf, SZ_1K,
			find_panel_seq_by_name(panel, debugfs->name));
	seq_printf(s, "%s\n", buf);
	kfree(buf);

	return 0;
}

static int panel_debug_sequence_store(struct panel_device *panel,
		char *seqname)
{
	return panel_do_seqtbl_by_name(panel, seqname);
}

static int panel_debug_resource_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	char *buf = kzalloc(SZ_1K, GFP_KERNEL);

	if (!buf)
		return 0;

	snprintf_resource(buf, SZ_1K,
			find_panel_resource(panel, debugfs->name));
	seq_printf(s, "%s\n", buf);
	kfree(buf);

	return 0;
}

static int panel_debug_resource_store(struct panel_device *panel,
		char *resname)
{
	return panel_resource_update_by_name(panel, resname);
}

static int panel_debug_property_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	char *buf = kzalloc(SZ_1K, GFP_KERNEL);

	if (!buf)
		return 0;

	snprintf_property(buf, SZ_1K,
			panel_find_property(panel, debugfs->name));
	seq_printf(s, "%s\n", buf);
	kfree(buf);

	return 0;
}

static int panel_debug_property_store(struct panel_device *panel,
		char *propname, int value)
{
	return panel_property_set_value(
			panel_find_property(panel, propname), value);
}

#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
static int panel_debug_freq_hop_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	struct panel_freq_hop *freq_hop = &panel->freq_hop;
	struct freq_hop_elem *elem;
	char buf[SZ_256] = { 0, };
	int i = 0;

	list_for_each_entry(elem, &freq_hop->head, list) {
		snprintf_freq_hop_elem(buf, SZ_256, elem);
		seq_printf(s, "%d: %s\n", i++, buf);
	}

	return 0;
}

static int panel_debug_fake_radio_noti(struct panel_device *panel, char *buf)
{
	struct dev_ril_bridge_msg msg;
	struct ril_noti_info rf_info;
	int rc;

	/*
	 * how to use
	 *
	 * tb74 { id = <N008>; range = <185000 188880>; dsi_freq = <1108000>; osc_freq = <1813000>; };
	 * tb75 { id = <N008>; range = <188881 191680>; dsi_freq = <1124000>; osc_freq = <1813000>; };
	 * tb76 { id = <N008>; range = <191681 191980>; dsi_freq = <1125000>; osc_freq = <1813000>; };
	 * #define N008		263
	 *
	 * echo 0 263 185000 > /d/panel/freq_hop
	 * echo 0 263 188881 > /d/panel/freq_hop
	 * echo 0 263 191681 > /d/panel/freq_hop
	 */

	if (!buf)
		return -EINVAL;

	rc = sscanf(buf, "%hhu %u %u",
			&rf_info.rat, &rf_info.band, &rf_info.channel);
	if (rc < 3) {
		panel_err("check your input. rc:(%d)\n", rc);
		return rc;
	}

	msg.dev_id = IPC_SYSTEM_CP_CHANNEL_INFO;
	msg.data_len = sizeof(struct ril_noti_info);
	msg.data = (void *)&rf_info;

	panel_info("radio_noti rat:%d band:%d channel:%d\n",
			rf_info.rat, rf_info.band, rf_info.channel);

	radio_notifier(&panel->freq_hop.radio_noti, 0, &msg);

	return 0;
}
#endif

#if defined(CONFIG_USDM_ADAPTIVE_MIPI)

enum {
	AD_MIPI_DBGFS_OPS_SET,
	AD_MIPI_DBGFS_OPS_CLEAR,
	AD_MIPI_DBGFS_OPS_EXEC,
	AP_MIPI_DBGFS_OPS_MAX
};

#define AD_MIPI_CMD_STR_SET	"set"
#define AD_MIPI_CMD_STR_CLEAR	"clear"
#define AD_MIPI_CMD_STR_EXECUTE	"execute"

struct adap_mipi_debuf_ops {
	char *cmd;
	int (*cmd_fn)(struct panel_adaptive_mipi *adap_mipi, char *buf);
};

#define BAND_INFO_ELEMENT_CNT 11

static int adap_mipi_dbgfs_fn_set(struct panel_adaptive_mipi *adap_mipi, char *buf)
{
	int ret;
	struct band_info band;
	struct cp_info *cp_info;
	struct band_info *dst_band;
	unsigned int temp[4];

	if (adap_mipi == NULL) {
		panel_err("null adapive mipi\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%u %u %u %u %u %i %i %i %u %u %i",
		&temp[0], &band.band, &band.channel, &temp[1], &band.bandwidth,
		&band.sinr, &band.rsrp, &band.rsrq, &temp[2], &temp[3], &band.pusch_power);
	if (ret != BAND_INFO_ELEMENT_CNT) {
		panel_err("adaptive mipi: invalid value\n");
		return -EINVAL;
	}
	band.rat = (u8)temp[0];
	band.connection_status = (u8)temp[1];
	band.cqi = (u8)temp[2];
	band.dl_mcs = (u8)temp[3];

	cp_info = &adap_mipi->debug_cp_info;
	if (cp_info->cell_count >= MAX_BAND) {
		panel_err("adaptive mipi: exceed band count\n");
		return -EINVAL;
	}
	dst_band = &cp_info->infos[cp_info->cell_count];
	memcpy(dst_band, &band, sizeof(struct band_info));

	panel_info("adaptive mipi: rat: %d, band: %d, channel: %d, conn_status: %d, bandwidth: %d\n",
		dst_band->rat, dst_band->band, dst_band->channel, dst_band->connection_status, dst_band->bandwidth);
	panel_info("adaptive mipi: sinr: %d, rsrp: %d, rsrq: %d, cqi: %d, dl_mcs: %d, pusch_power: %d\n",
		dst_band->sinr, dst_band->rsrp, dst_band->rsrq, dst_band->cqi, dst_band->dl_mcs, dst_band->pusch_power);

	cp_info->cell_count++;

	return 0;
}

static int adap_mipi_dbgfs_fn_clear(struct panel_adaptive_mipi *adap_mipi, char *buf)
{
	struct cp_info *cp_info;

	if (adap_mipi == NULL) {
		panel_err("null adapive mipi\n");
		return -EINVAL;
	}
	cp_info = &adap_mipi->debug_cp_info;

	panel_info("adaptive mipi: debug clear: cell_count: %d -> 0\n", cp_info->cell_count);

	cp_info->cell_count = 0;

	return 0;
}

static int adap_mipi_dbgfs_fn_execute(struct panel_adaptive_mipi *adap_mipi, char *buf)
{
	int ret;
	struct cp_info *cp_info;
	struct dev_ril_bridge_msg ril_msg;

	if (adap_mipi == NULL) {
		panel_err("null adapive mipi\n");
		return -EINVAL;
	}
	cp_info = &adap_mipi->debug_cp_info;
	if (cp_info->cell_count == 0) {
		panel_err("adaptive mipi: invalid debug cell count: %d\n", cp_info->cell_count);
		return -EINVAL;
	}
	panel_info("adaptive mipi: execute\n");

	ril_msg.dev_id = SUB_CMD_ADAPTIVE_MIPI;
	ril_msg.data = (void *)&adap_mipi->debug_cp_info;

	if (adap_mipi->radio_noti.notifier_call) {
		ret = unregister_dev_ril_bridge_event_notifier(&adap_mipi->radio_noti);
		panel_info("adaptive mipi: unregister ril bridge ret: %d\n", ret);
		adap_mipi->radio_noti.notifier_call = NULL;
	}

	ril_notifier(&adap_mipi->radio_noti, 0, &ril_msg);

	return 0;
}


static int panel_debug_adaptive_mipi(struct panel_device *panel, char *buf)
{
	int i, ret = -EINVAL;
	struct panel_adaptive_mipi *adap_mipi;
	struct adap_mipi_debuf_ops debug_ops[AP_MIPI_DBGFS_OPS_MAX] = {
		[AD_MIPI_DBGFS_OPS_SET] = {.cmd = AD_MIPI_CMD_STR_SET, .cmd_fn = adap_mipi_dbgfs_fn_set},
		[AD_MIPI_DBGFS_OPS_CLEAR] = {.cmd = AD_MIPI_CMD_STR_CLEAR, .cmd_fn = adap_mipi_dbgfs_fn_clear},
		[AD_MIPI_DBGFS_OPS_EXEC] = {.cmd = AD_MIPI_CMD_STR_EXECUTE, .cmd_fn = adap_mipi_dbgfs_fn_execute},
	};

	if (panel == NULL) {
		panel_err("null panel\n");
		return -EINVAL;
	}
	adap_mipi = &panel->adap_mipi;

	for (i = 0; i < AP_MIPI_DBGFS_OPS_MAX; i++) {
		if ((strncmp(debug_ops[i].cmd, buf, strlen(debug_ops[i].cmd)) == 0)
			&& (debug_ops[i].cmd_fn != NULL)) {
			ret = debug_ops[i].cmd_fn(adap_mipi, strstrip(buf + strlen(debug_ops[i].cmd)));

			return ret;
		}
	}

	panel_err("adaptive mipi: invalid command\n");

	return ret;

}


#define BAND_INFO_DEBUG_BUF_SZ	256

static size_t sprinf_band_info(int index, char *buf, struct band_info *band_msg)
{
	size_t len;

	if (band_msg == NULL)
		return 0;

	len = snprintf(buf, BAND_INFO_DEBUG_BUF_SZ, "[%2d] rat:%d, band:%d, channel:%d, status:%d, bw:%d, sinr:%d",
		index, band_msg->rat, band_msg->band, band_msg->channel, band_msg->connection_status,
		band_msg->bandwidth, band_msg->sinr);

	len += sprintf(buf + len, "\n");

	return len;
}

static int panel_debug_adaptive_mipi_show(struct seq_file *s)
{
	int i, len = 0;
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	struct panel_adaptive_mipi *adap_mipi;
	struct cp_info *cp_info;
	char *buf;

	if (panel == NULL) {
		panel_err("null panel\n");
		return -EINVAL;
	}

	adap_mipi = &panel->adap_mipi;
	cp_info = &adap_mipi->debug_cp_info;

	if (cp_info->cell_count <= 0)
		return 0;

	buf = kzalloc(BAND_INFO_DEBUG_BUF_SZ * cp_info->cell_count, GFP_KERNEL);
	if (buf == NULL) {
		panel_err("adaptive mipi: alloc failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < cp_info->cell_count; i++)
		len += sprinf_band_info(i, buf + len, &cp_info->infos[i]);
	buf[len] = 0;

	seq_printf(s, "%s", buf);

	kfree(buf);

	return 0;
}
#endif


#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
static int panel_debug_panel_event_show(struct seq_file *s)
{
	seq_printf(s, "echo <event> <state> > /d/panel/panel_event\n");

	return 0;
}

static int panel_debug_panel_event_noti(struct panel_device *panel, char *buf)
{
	int rc, event, state;
	struct panel_notifier_event_data evt_data = {
		.display_index = 0U,
	};

	rc = sscanf(buf, "%d %d", &event, &state);
	if (rc < 2) {
		panel_err("check your input. rc:(%d)\n", rc);
		return rc;
	}

	if (event < 0 || event >= MAX_PANEL_EVENT) {
		panel_err("invalid panel event(%d)\n", event);
		return -EINVAL;
	}

	if (state < PANEL_EVENT_STATE_NONE ||
		event >= MAX_PANEL_EVENT_STATE) {
		panel_err("invalid panel event state(%d)\n", state);
		return -EINVAL;
	}
	evt_data.state = state;

	panel_notifier_call_chain(event, &evt_data);
	panel_info("event:%d state:%d\n", event, evt_data.state);

	return 0;
}
#endif

#if defined(CONFIG_USDM_PANEL_JSON)
char *json_buffer;
int panel_debug_ezop_jsonw_all(struct panel_device *panel, struct seq_file *s)
{
	json_writer_t *w;
	char *p;
	int i;

	json_buffer = kvmalloc(SZ_16M, GFP_KERNEL);
	if (!json_buffer) {
		panel_err("failed to alloc json buffer\n");
		return -ENOMEM;
	}

	w = jsonw_new(json_buffer, SZ_16M);
	if (!w) {
		panel_err("failed to jsonw_new\n");
		return -EINVAL;
	}

	jsonw_pretty(w, true);
	jsonw_start_object(w);
	for (i = CMD_TYPE_PROP; i < MAX_CMD_TYPE; i++)
		jsonw_sorted_pnobj_list(w, i, panel_get_object_list(panel, i));
	jsonw_end_object(w);
	pr_info("dump json done\n");

	while ((p = strsep(&json_buffer, "\n")) != NULL) {
		if (!*p)
			continue;

		seq_printf(s, "%s\n", p);
	}

	jsonw_destroy(&w);
	kvfree(json_buffer);

	return 0;
}

static int panel_debug_ezop_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	int ret;

	ret = panel_debug_ezop_jsonw_all(panel, s);
	if (ret < 0)
		return 0;

	return 0;
}

static int panel_debug_ezop_load_json(struct panel_device *panel,
		char *json_filename)
{
	int ret;
	struct list_head new_pnobj_list;

	INIT_LIST_HEAD(&new_pnobj_list);

	if (!strncmp(json_filename, PANEL_BUILT_IN_FW_NAME,
				strlen(PANEL_BUILT_IN_FW_NAME) + 1)) {
		if (!panel->panel_data.ezop_json) {
			panel_err("no such firmware(%s)\n", PANEL_BUILT_IN_FW_NAME);
			return 0;
		}
		ret = panel_firmware_load(panel,
				NULL, panel->panel_data.ezop_json, &new_pnobj_list);
	} else {
		ret = panel_firmware_load(panel,
				json_filename, NULL, &new_pnobj_list);
		if (ret == -ENOENT)
			panel_err("no such firmware(%s)\n", json_filename);
	}

	if (ret < 0)
		return 0;

	return panel_reprobe_with_pnobj_list(panel, &new_pnobj_list);
}

static int panel_debug_firmware_show(struct seq_file *s)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_device *panel = debugfs->private;
	struct timespec64 cur_ts, load_ts, delta_ts;
	u64 load_time_ms, elapsed_ms;

	ktime_get_ts64(&cur_ts);
	load_ts = panel_firmware_get_load_time(panel);
	load_time_ms = timespec64_to_ns(&cur_ts) / 1000000;
	delta_ts = timespec64_sub(cur_ts, load_ts);
	elapsed_ms = timespec64_to_ns(&delta_ts) / 1000000;

	seq_printf(s, "file: %s\n", panel_firmware_get_name(panel));
	seq_printf(s, "time: %lld.%lld sec (%lld.%lld sec ago)\n",
			load_time_ms / 1000UL, load_time_ms % 1000UL / 100UL,
			elapsed_ms / 1000UL, elapsed_ms % 1000UL / 100UL);
	seq_printf(s, "count: %d\n", panel_firmware_get_load_count(panel));
	seq_printf(s, "status: %s\n", is_panel_firmwarel_load_success(panel) ? "OK" : "NOK");
	seq_printf(s, "checksum: 0x%08llx\n", panel_firmware_get_csum(panel));

	return 0;
}
#endif

static int panel_debug_simple_show(struct seq_file *s, void *unused)
{
	struct panel_debugfs *debugfs = s->private;
	struct panel_debugfs *parent = debugfs->parent;
	int id;

	if (!parent)
		return -EINVAL;

	id = string_to_debugfs_dir_id(parent->name);
	if (id == PANEL_DEBUGFS_DIR_SEQUENCE)
		return panel_debug_sequence_show(s);
	else if (id == PANEL_DEBUGFS_DIR_RESOURCE)
		return panel_debug_resource_show(s);
	else if (id == PANEL_DEBUGFS_DIR_PROPERTY)
		return panel_debug_property_show(s);

	id = string_to_debugfs_file_id(debugfs->name);
	if (id < 0)
		return -EINVAL;

	switch (id) {
	case PANEL_DEBUGFS_FILE_LOG:
		panel_debug_log_show(s);
		break;
	case PANEL_DEBUGFS_FILE_CMD_LOG:
		panel_debug_cmd_log_show(s);
		break;
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	case PANEL_DEBUGFS_FILE_FREQ_HOP:
		panel_debug_freq_hop_show(s);
		break;
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	case PANEL_DEBUGFS_FILE_PANEL_EVENT:
		panel_debug_panel_event_show(s);
		break;
#endif
#if defined(CONFIG_USDM_PANEL_JSON)
	case PANEL_DEBUGFS_FILE_EZOP:
		panel_debug_ezop_show(s);
		break;
	case PANEL_DEBUGFS_FILE_FIRMWARE:
		panel_debug_firmware_show(s);
		break;
#endif
#if defined(CONFIG_USDM_ADAPTIVE_MIPI)
	case PANEL_DEBUGFS_FILE_ADAPTIVE_MIPI:
		panel_debug_adaptive_mipi_show(s);
		break;
#endif
	default:
		break;
	}

	return 0;
}

static ssize_t panel_debug_simple_write(struct file *file,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	struct seq_file *s = file->private_data;
	struct panel_debugfs *debugfs = s->private;
	struct panel_debugfs *parent = debugfs->parent;
	struct panel_device *panel = debugfs->private;
	char argbuf[SZ_256];
	int rc = 0, res = 0, id;

	if (!parent)
		return -EINVAL;

	id = string_to_debugfs_dir_id(parent->name);
	if (id >= 0) {
		if (id == PANEL_DEBUGFS_DIR_SEQUENCE) {
			rc = panel_debug_sequence_store(panel, debugfs->name);
			if (rc)
				return rc;
		} else if (id == PANEL_DEBUGFS_DIR_RESOURCE) {
			rc = panel_debug_resource_store(panel, debugfs->name);
			if (rc)
				return rc;
		} else if (id == PANEL_DEBUGFS_DIR_PROPERTY) {
			rc = kstrtoint_from_user(buf, count, 10, &res);
			if (rc)
				return rc;
			rc = panel_debug_property_store(panel, debugfs->name, res);
			if (rc)
				return rc;
		}
		return count;
	}

	id = string_to_debugfs_file_id(debugfs->name);
	if (id < 0)
		return -EINVAL;

	switch (id) {
	case PANEL_DEBUGFS_FILE_LOG:
		rc = kstrtoint_from_user(buf, count, 10, &res);
		if (rc)
			return rc;

		panel_log_level = res;
		panel_info("panel_log_level: %d\n", panel_log_level);
		break;
	case PANEL_DEBUGFS_FILE_CMD_LOG:
		rc = kstrtoint_from_user(buf, count, 10, &res);
		if (rc)
			return rc;

		panel_cmd_log = res;
		panel_info("panel_cmd_log: %d\n", panel_cmd_log);
		break;
#if defined(CONFIG_USDM_PANEL_FREQ_HOP)
	case PANEL_DEBUGFS_FILE_FREQ_HOP:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_fake_radio_noti(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
#endif
#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	case PANEL_DEBUGFS_FILE_PANEL_EVENT:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_panel_event_noti(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
#endif
#if defined(CONFIG_USDM_PANEL_JSON)
	case PANEL_DEBUGFS_FILE_EZOP:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_ezop_load_json(panel, strstrip(argbuf));
		if (rc)
			return rc;

		break;
#endif
#if defined(CONFIG_USDM_ADAPTIVE_MIPI)
	case PANEL_DEBUGFS_FILE_ADAPTIVE_MIPI:
		if (count >= ARRAY_SIZE(argbuf))
			return -EINVAL;

		if (copy_from_user(argbuf, buf, count))
			return -EOVERFLOW;

		argbuf[count] = '\0';
		rc = panel_debug_adaptive_mipi(panel, strstrip(argbuf));
		if (rc)
			return rc;
	break;
#endif
	default:
		break;
	}

	return count;
}

static int panel_debug_simple_open(struct inode *inode, struct file *file)
{
	return single_open(file, panel_debug_simple_show, inode->i_private);
}

static const struct file_operations panel_debugfs_simple_fops = {
	.open = panel_debug_simple_open,
	.write = panel_debug_simple_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static struct panel_debugfs *
panel_create_debugfs_dir(struct panel_device *panel,
		char *name, struct panel_debugfs *dir)
{
	struct panel_debugfs *debugfs;

	debugfs = kzalloc(sizeof(struct panel_debugfs), GFP_KERNEL);
	if (!debugfs)
		return NULL;

	debugfs->private = panel;
	strncpy(debugfs->name, name, MAX_PANEL_DEBUGFS_NAME_SIZE);
	debugfs->name[MAX_PANEL_DEBUGFS_NAME_SIZE-1] = '\0';
	debugfs->parent = dir;
	debugfs->dir = debugfs_create_dir(name, dir ? dir->dir : NULL);
	if (!debugfs->dir) {
		panel_err("failed to create debugfs dir(%s)\n", name);
		kfree(debugfs);
		return NULL;
	}
	INIT_LIST_HEAD(&debugfs->dirs);
	if (dir)
		list_add_tail(&debugfs->list, &dir->dirs);
	panel_dbg("dir:%s\n", name);

	return debugfs;
}

static struct panel_debugfs *
panel_create_debugfs_file(struct panel_device *panel,
		char *name, struct panel_debugfs *dir)
{
	struct panel_debugfs *debugfs;

	if (!dir) {
		panel_err("directory is null\n");
		return NULL;
	}

	debugfs = kzalloc(sizeof(struct panel_debugfs), GFP_KERNEL);
	if (!debugfs)
		return NULL;

	debugfs->private = panel;
	strncpy(debugfs->name, name, MAX_PANEL_DEBUGFS_NAME_SIZE);
	debugfs->name[MAX_PANEL_DEBUGFS_NAME_SIZE-1] = '\0';
	debugfs->parent = dir;
	debugfs->file = debugfs_create_file(name, 0660,
			dir->dir, debugfs, &panel_debugfs_simple_fops);
	if (!debugfs->file) {
		panel_err("failed to create debugfs file(%s)\n", name);
		kfree(debugfs);
		return NULL;
	}
	INIT_LIST_HEAD(&debugfs->dirs);
	list_add_tail(&debugfs->list, &dir->dirs);
	panel_dbg("file:%s\n", name);

	return debugfs;
}

int panel_create_pnobjs_debugfs(struct panel_device *panel,
		char *name, struct panel_debugfs *root)
{
	struct list_head *head = NULL;
	struct panel_debugfs *entry;
	struct panel_debugfs *dir;
	struct pnobj *pos;
	int id, cmd_type = CMD_TYPE_NONE;

	dir = panel_create_debugfs_dir(panel, name, root);
	if (!dir)
		return -ENOENT;

	id = string_to_debugfs_dir_id(dir->name);
	if (id < 0) {
		panel_warn("debugfs_dir(%s) not found\n",
				dir->name);
		return -EINVAL;
	}

	if (id == PANEL_DEBUGFS_DIR_SEQUENCE)
		cmd_type = CMD_TYPE_SEQ;
	else if (id == PANEL_DEBUGFS_DIR_RESOURCE)
		cmd_type = CMD_TYPE_RES;
	else if (id == PANEL_DEBUGFS_DIR_PROPERTY)
		cmd_type = CMD_TYPE_PROP;

	if (cmd_type == CMD_TYPE_NONE) {
		panel_warn("cmd type none\n");
		return -EINVAL;
	}

	head = panel_get_object_list(panel, cmd_type);
	if (!head) {
		panel_warn("failed to get pnobj(%s) list\n",
				cmd_type_to_string(cmd_type));
		return -EINVAL;
	}

	list_for_each_entry(pos, head, list) {
		entry = panel_create_debugfs_file(panel,
				get_pnobj_name(pos), dir);
		if (!entry)
			return -ENOENT;
	}

	return 0;
}

int panel_create_debugfs(struct panel_device *panel)
{
	struct panel_debugfs *entry;
	char name[MAX_PANEL_DEBUGFS_NAME_SIZE];
	int i;

	if (!panel)
		return -EINVAL;

	if (panel->id == 0)
		snprintf(name, MAX_PANEL_DEBUGFS_NAME_SIZE, "panel");
	else
		snprintf(name, MAX_PANEL_DEBUGFS_NAME_SIZE, "panel%d", panel->id);

	panel->d.root = panel_create_debugfs_dir(panel, name, NULL);
	if (!panel->d.root) {
		panel_err("failed to create debugfs directory(%s).\n", name);
		return -ENOENT;
	}

	for (i = 0; i < MAX_PANEL_DEBUGFS_FILE; i++) {
		entry = panel_create_debugfs_file(panel,
				(char *)panel_debugfs_file_name[i], panel->d.root);
		if (!entry)
			return -ENOENT;
	}

	return 0;
}

void panel_destroy_debugfs_entry(struct panel_debugfs *debugfs)
{
	struct panel_debugfs *pos, *next;

	if (!debugfs)
		return;

	list_for_each_entry_safe(pos, next, &debugfs->dirs, list)
		panel_destroy_debugfs_entry(pos);

	if (debugfs->dir)
		debugfs_remove(debugfs->dir);
	if (debugfs->file)
		debugfs_remove(debugfs->file);
	list_del(&debugfs->list);
	kfree(debugfs);
}

void panel_destroy_debugfs(struct panel_device *panel)
{
	panel_destroy_debugfs_entry(panel->d.root);
}

int panel_create_panel_object_debugfs(struct panel_device *panel)
{
	int i, ret;

	for (i = 0; i < MAX_PANEL_DEBUGFS_DIR; i++) {
		ret = panel_create_pnobjs_debugfs(panel,
				(char *)panel_debugfs_dir_name[i], panel->d.root);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void panel_destroy_panel_object_debugfs(struct panel_device *panel)
{
	struct panel_debugfs *pos, *next;

	list_for_each_entry_safe(pos, next, &panel->d.root->dirs, list) {
		if (string_to_debugfs_dir_id(pos->name) >= 0)
			panel_destroy_debugfs_entry(pos);
	}
}
