/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/** Implements */
#include "mxlogger.h"

/** Uses */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/version.h>
#include <scsc/scsc_logring.h>
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

#include "srvman.h"
#include "scsc_mif_abs.h"
#include "miframman.h"
#include "mifintrbit.h"
#include "mxmgmt_transport.h"

static bool mxlogger_disabled;
module_param(mxlogger_disabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_disabled, "Disable MXLOGGER Configuration. Effective only at next WLBT boot.");

static bool mxlogger_manual_layout;
module_param(mxlogger_manual_layout, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_layout, "User owns the buffer layout. Only sync buffer will be allocated");

static int mxlogger_manual_total_mem = MXL_POOL_SZ_WLAN - MXLOGGER_SYNC_SIZE - sizeof(struct mxlogger_config_area);
module_param(mxlogger_manual_total_mem, int, S_IRUGO);
MODULE_PARM_DESC(mxlogger_manual_total_mem, "Available memory when mxlogger_manual_layout is enabled");

static int mxlogger_manual_imp;
module_param(mxlogger_manual_imp, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_imp, "size for IMP buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_common;
module_param(mxlogger_manual_rsv_common, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_common, "size for RSV COMMON buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_bt;
module_param(mxlogger_manual_rsv_bt, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_bt, "size for RSV BT buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_wlan = MXL_POOL_SZ_WLAN - MXLOGGER_SYNC_SIZE - sizeof(struct mxlogger_config_area);
module_param(mxlogger_manual_rsv_wlan, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_wlan, "size for RSV WLAN buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_radio;
module_param(mxlogger_manual_rsv_radio, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_radio, "size for RSV RADIO buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_mxlog;
module_param(mxlogger_manual_mxlog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mmxlogger_manual_mxlog, "size for MXLOG buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_udi;
module_param(mxlogger_manual_udi, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_udi, "size for UDI buffer when mxlogger_manual_layout is enabled");

static bool mxlogger_forced_to_host;

static void update_fake_observer(void)
{
	static bool mxlogger_fake_observers_registered;

	if (mxlogger_forced_to_host) {
		if (!mxlogger_fake_observers_registered) {
			mxlogger_register_global_observer("FAKE_OBSERVER");
			mxlogger_fake_observers_registered = true;
		}
		SCSC_TAG_INFO(MXMAN, "MXLOGGER is now FORCED TO HOST.\n");
	} else {
		if (mxlogger_fake_observers_registered) {
			mxlogger_unregister_global_observer("FAKE_OBSERVER");
			mxlogger_fake_observers_registered = false;
		}
		SCSC_TAG_INFO(MXMAN, "MXLOGGER is now operating NORMALLY.\n");
	}
}

static int mxlogger_force_to_host_set_param_cb(const char *val, const struct kernel_param *kp)
{
	bool nval;

	if (!val || strtobool(val, &nval))
		return -EINVAL;

	if (mxlogger_forced_to_host ^ nval) {
		mxlogger_forced_to_host = nval;
		update_fake_observer();
	}
	return 0;
}

/**
 * As described in struct kernel_param+ops the _get method:
 * -> returns length written or -errno.  Buffer is 4k (ie. be short!)
 */
static int mxlogger_force_to_host_get_param_cb(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%c", mxlogger_forced_to_host ? 'Y' : 'N');
}

static struct kernel_param_ops mxlogger_force_to_host_ops = {
	.set = mxlogger_force_to_host_set_param_cb,
	.get = mxlogger_force_to_host_get_param_cb,
};
module_param_cb(mxlogger_force_to_host, &mxlogger_force_to_host_ops, NULL, 0644);
MODULE_PARM_DESC(mxlogger_force_to_host, "Force mxlogger to redirect to Host all the time, using a fake observer.");

static u8 active_global_observers;
static DEFINE_MUTEX(global_lock);

struct mxlogger_node {
	struct list_head list;
	struct mxlogger *mxl;
};
static struct mxlogger_list {
	struct list_head list;
} mxlogger_list = { .list = LIST_HEAD_INIT(mxlogger_list.list) };

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int mxlogger_collect_init(struct scsc_log_collector_client *collect_client);
static int mxlogger_collect_init_wpan(struct scsc_log_collector_client *collect_client);
static int mxlogger_collect(struct scsc_log_collector_client *collect_client, size_t size);
static int mxlogger_collect_wpan(struct scsc_log_collector_client *collect_client, size_t size);
static int mxlogger_collect_end(struct scsc_log_collector_client *collect_client);
static int mxlogger_collect_end_wpan(struct scsc_log_collector_client *collect_client);

/* Collect client registration SYNC buffer */
/* SYNC - SHOULD BE THE FIRST CHUNK TO BE CALLED - SO USE THE INIT/END ON THIS CLIENT */
static struct scsc_log_collector_client mxlogger_collect_client_sync = {
	.name = "Sync",
	.type = SCSC_LOG_CHUNK_SYNC,
	.collect_init = mxlogger_collect_init,
	.collect = mxlogger_collect,
	.collect_end = mxlogger_collect_end,
	.prv = NULL,
};

/* Collect client registration IMP buffer */
static struct scsc_log_collector_client mxlogger_collect_client_imp = {
	.name = "Important",
	.type = SCSC_LOG_CHUNK_IMP,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_common = {
	.name = "Rsv_common",
	.type = SCSC_LOG_RESERVED_COMMON,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_bt = {
	.name = "Rsv_bt",
	.type = SCSC_LOG_RESERVED_BT,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_wlan = {
	.name = "Rsv_wlan",
	.type = SCSC_LOG_RESERVED_WLAN,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_radio = {
	.name = "Rsv_radio",
	.type = SCSC_LOG_RESERVED_RADIO,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};
/* Collect client registration MXL buffer */
static struct scsc_log_collector_client mxlogger_collect_client_mxl = {
	.name = "MXL",
	.type = SCSC_LOG_CHUNK_MXL,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

/* Collect client registration MXL buffer */
static struct scsc_log_collector_client mxlogger_collect_client_udi = {
	.name = "UDI",
	.type = SCSC_LOG_CHUNK_UDI,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_sync_wpan = {
	.name = "Sync_WPAN",
	.type = SCSC_LOG_CHUNK_SYNC_WPAN,
	.collect_init = mxlogger_collect_init_wpan,
	.collect = mxlogger_collect_wpan,
	.collect_end = mxlogger_collect_end_wpan,
	.prv = NULL,
};

/* Collect client registration IMP buffer */
static struct scsc_log_collector_client mxlogger_collect_client_imp_wpan = {
	.name = "Important_WPAN",
	.type = SCSC_LOG_CHUNK_IMP_WPAN,
	.collect_init = NULL,
	.collect = mxlogger_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_common_wpan = {
	.name = "Rsv_common_WPAN",
	.type = SCSC_LOG_RESERVED_COMMON_WPAN,
	.collect_init = NULL,
	.collect = mxlogger_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_bt_wpan = {
	.name = "Rsv_bt_WPAN",
	.type = SCSC_LOG_RESERVED_BT_WPAN,
	.collect_init = NULL,
	.collect = mxlogger_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_wlan_wpan = {
	.name = "Rsv_wlan_WPAN",
	.type = SCSC_LOG_RESERVED_WLAN_WPAN,
	.collect_init = NULL,
	.collect = mxlogger_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_radio_wpan = {
	.name = "Rsv_radio_WPAN",
	.type = SCSC_LOG_RESERVED_RADIO_WPAN,
	.collect_init = NULL,
	.collect = mxlogger_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};
/* Collect client registration MXL buffer */
static struct scsc_log_collector_client mxlogger_collect_client_mxl_wpan = {
	.name = "MXL_WPAN",
	.type = SCSC_LOG_CHUNK_MXL_WPAN,
	.collect_init = NULL,
	.collect = mxlogger_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};

/* Collect client registration MXL buffer */
static struct scsc_log_collector_client mxlogger_collect_client_udi_wpan = {
	.name = "UDI_WPAN",
	.type = SCSC_LOG_CHUNK_UDI_WPAN,
	.collect_init = NULL,
	.collect = mxlogger_collect_wpan,
	.collect_end = NULL,
	.prv = NULL,
};
#endif

static const char *const mxlogger_buf_name[] = { "syn",      "imp",       "rsv_common", "rsv_bt",
						 "rsv_wlan", "rsv_radio", "mxl",	"udi" };

static void __mxlogger_message_handler(struct mxlogger *mxlogger, struct mxlogger_channel *chan, const void *message)
{
	const struct log_msg_packet *msg = message;
	u16 reason_code;

	switch (msg->msg) {
	case MM_MXLOGGER_INITIALIZED_EVT:
		SCSC_TAG_INFO(MXMAN, "MXLOGGER Initialized.\n");
		mxlogger->initialized = true;
		complete(&chan->rings_serialized_ops);
		break;
	case MM_MXLOGGER_STARTED_EVT:
		SCSC_TAG_INFO(MXMAN, "MXLOGGER:: RINGS Enabled.\n");
		mxlogger->enabled = true;
		complete(&chan->rings_serialized_ops);
		break;
	case MM_MXLOGGER_STOPPED_EVT:
		SCSC_TAG_INFO(MXMAN, "MXLOGGER:: RINGS Disabled.\n");
		mxlogger->enabled = false;
		complete(&chan->rings_serialized_ops);
		break;
	case MM_MXLOGGER_COLLECTION_FW_REQ_EVT:
		/* If arg is zero, FW is using the 16bit reason code API */
		/* therefore, the reason code is in the payload */
		if (msg->arg == 0x00)
			memcpy(&reason_code, &msg->payload[0], sizeof(u16));
		else
			/* old API */
			reason_code = msg->arg;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		SCSC_TAG_INFO(MXMAN, "MXLOGGER:: FW requested collection - Reason code:0x%04x\n", reason_code);
		scsc_log_collector_schedule_collection(SCSC_LOG_FW, reason_code);
#endif
		break;
	default:
		SCSC_TAG_ERR(MXMAN, "Received UNKNOWN msg on MMTRANS_CHAN_ID_MAXWELL_LOGGING -- msg->msg=%d\n",
				 msg->msg);
		break;
	}
}

static void mxlogger_message_handler(const void *message, void *data)
{
	struct mxlogger_channel *chan = (struct mxlogger_channel *)data;
	struct mxlogger *mxlogger = chan->mxlogger;

	mutex_lock(&mxlogger->chan_lock);
	__mxlogger_message_handler(mxlogger, chan, message);
	mutex_unlock(&mxlogger->chan_lock);
}

static void mxlogger_message_handler_wpan(const void *message, void *data)
{
	struct mxlogger_channel *chan = (struct mxlogger_channel *)data;
	struct mxlogger *mxlogger = chan->mxlogger;

	mutex_lock(&mxlogger->chan_lock);
	__mxlogger_message_handler(mxlogger, chan, message);
	mutex_unlock(&mxlogger->chan_lock);
}

static int __mxlogger_generate_sync_record(struct mxlogger *mxlogger, u8 channel, enum mxlogger_sync_event event)
{
	struct mxlogger_sync_record *sync_r_mem;
	struct mxmgmt_transport *mxmgmt_transport;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct timespec64 ts;
#else
	struct timeval t;
#endif
	struct mxlogger_channel *chan = &mxlogger->chan[channel];
	struct log_msg_packet msg = {};
	unsigned long int jd;
	void *mem;
	ktime_t t1, t2;

	if (!chan->enabled) {
		SCSC_TAG_INFO(MXMAN, "Channel %s disabled\n", (chan->target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");
		return -EIO;
	}

	/* Assume mxlogger->lock mutex is held */
	if (!mxlogger || !mxlogger->configured)
		return -EIO;

	if (chan->target == SCSC_MIF_ABS_TARGET_WLAN)
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(mxlogger->mx);
	else
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(mxlogger->mx);

	msg.msg = MM_MXLOGGER_SYNC_RECORD;
	msg.arg = MM_MXLOGGER_SYNC_INDEX;
	memcpy(&msg.payload, &chan->sync_buffer_index, sizeof(chan->sync_buffer_index));

	/* Get the pointer from the index of the sync array */
	mem = chan->mem_sync_buf + chan->sync_buffer_index * sizeof(struct mxlogger_sync_record);
	sync_r_mem = (struct mxlogger_sync_record *)mem;
	/* Write values in record as FW migth be doing sanity checks */
	sync_r_mem->tv_sec = 1;
	sync_r_mem->tv_usec = 1;
	sync_r_mem->kernel_time = 1;
	sync_r_mem->sync_event = event;
	sync_r_mem->fw_time = 0;
	sync_r_mem->fw_wrap = 0;

	SCSC_TAG_INFO(MXMAN, "Get FW %s time\n", (chan->target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");
	preempt_disable();
	/* set the tight loop timeout - we do not require precision but something to not
	 * loop forever
	 */
	jd = jiffies + msecs_to_jiffies(20);
	/* Send the msg as fast as possible */
	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING, &msg, sizeof(msg));
	t1 = ktime_get();
	/* Tight loop to read memory */
	while (time_before(jiffies, jd) && sync_r_mem->fw_time == 0 && sync_r_mem->fw_wrap == 0)
		;
	t2 = ktime_get();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ktime_get_real_ts64(&ts);
#else
	do_gettimeofday(&t);
#endif
	preempt_enable();

	/* Do the processing */
	if (sync_r_mem->fw_wrap == 0 && sync_r_mem->fw_time == 0) {
		/* FW didn't update the record (FW panic?) */
		SCSC_TAG_INFO(MXMAN, "FW failure updating the FW time\n");
		SCSC_TAG_INFO(MXMAN, "Sync delta %lld\n", ktime_to_ns(ktime_sub(t2, t1)));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		sync_r_mem->tv_sec = ts.tv_sec;
		sync_r_mem->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
#else
		sync_r_mem->tv_sec = (u64)t.tv_sec;
		sync_r_mem->tv_usec = (u64)t.tv_usec;
#endif
		sync_r_mem->kernel_time = ktime_to_ns(t2);
		sync_r_mem->sync_event = event;
		return 0;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	sync_r_mem->tv_sec = ts.tv_sec;
	sync_r_mem->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
#else
	sync_r_mem->tv_sec = (u64)t.tv_sec;
	sync_r_mem->tv_usec = (u64)t.tv_usec;
#endif
	sync_r_mem->kernel_time = ktime_to_ns(t2);
	sync_r_mem->sync_event = event;

	SCSC_TAG_INFO(MXMAN, "Sample, %lld, %u, %lld.%06lld\n", ktime_to_ns(sync_r_mem->kernel_time),
		      sync_r_mem->fw_time, sync_r_mem->tv_sec, sync_r_mem->tv_usec);
	SCSC_TAG_INFO(MXMAN, "Sync delta %lld\n", ktime_to_ns(ktime_sub(t2, t1)));

	chan->sync_buffer_index++;
	chan->sync_buffer_index &= SYNC_MASK;

	return 0;
}

/* generate a sync record on all channels */
int mxlogger_generate_sync_record(struct mxlogger *mxlogger, enum mxlogger_sync_event event)
{
	u8 i;

	if (mxlogger->configured == false) {
		SCSC_TAG_INFO(MXMAN, "Mxlogger not configured\n");
		return -EIO;
	}

	mutex_lock(&mxlogger->lock);

	for (i = 0; i < MXLOGGER_CHANNELS; i++)
		__mxlogger_generate_sync_record(mxlogger, i, event);

	mutex_unlock(&mxlogger->lock);

	return 0;
}

static void mxlogger_wait_for_msg_reinit_completion(struct mxlogger_channel *chan)
{
	reinit_completion(&chan->rings_serialized_ops);
}

static bool mxlogger_wait_for_msg_reply(struct mxlogger_channel *chan)
{
	int ret;

	ret = wait_for_completion_timeout(&chan->rings_serialized_ops, usecs_to_jiffies(MXLOGGER_RINGS_TMO_US));
	if (ret) {
		int i;

		SCSC_TAG_DBG3(MXMAN, "MXLOGGER RINGS -- replied in %lu usecs.\n",
			      MXLOGGER_RINGS_TMO_US - jiffies_to_usecs(ret));

		for (i = 0; i < MXLOGGER_NUM_BUFFERS; i++)
			SCSC_TAG_DBG3(MXMAN, "MXLOGGER:: RING[%d] -- INFO[0x%X]  STATUS[0x%X]\n", i,
				      chan->cfg->bfds[i].info, chan->cfg->bfds[i].status);
	} else {
		SCSC_TAG_ERR(MXMAN, "MXLOGGER timeout waiting for reply.\n");
	}

	return ret ? true : false;
}

static inline void __mxlogger_enable(struct mxlogger *mxlogger, u8 channel, bool enable, uint8_t reason)
{
	struct log_msg_packet msg = {};
	struct mxmgmt_transport *mxmgmt_transport;
	struct mxlogger_channel *chan = &mxlogger->chan[channel];

	if (!chan->enabled) {
		SCSC_TAG_INFO(MXMAN, "Channel %s disabled\n", channel ? "WPAN" : "WLAN");
		return;
	}

	if (chan->target == SCSC_MIF_ABS_TARGET_WLAN)
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(mxlogger->mx);
	else
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(mxlogger->mx);

	msg.msg = MM_MXLOGGER_LOGGER_CMD;
	msg.arg = (enable) ? MM_MXLOGGER_LOGGER_ENABLE : MM_MXLOGGER_LOGGER_DISABLE;
	msg.payload[0] = reason;

	/* Reinit the completion before sending the message over cpacketbuffer
	 * otherwise there might be a race condition
	 */
	mxlogger_wait_for_msg_reinit_completion(chan);

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING, &msg, sizeof(msg));

	SCSC_TAG_DBG4(MXMAN, "MXLOGGER RINGS -- enable:%d  reason:%d\n", enable, reason);

	mxlogger_wait_for_msg_reply(chan);
}

static void mxlogger_enable_channel(struct mxlogger *mxlogger, bool enable, u8 channel)
{
	__mxlogger_enable(mxlogger, channel, enable, MM_MXLOGGER_DISABLE_REASON_STOP);
}

static void mxlogger_enable(struct mxlogger *mxlogger, bool enable)
{
	u8 i;

	for (i = 0; i < MXLOGGER_CHANNELS; i++)
		__mxlogger_enable(mxlogger, i, enable, MM_MXLOGGER_DISABLE_REASON_STOP);
}

static int mxlogger_send_config(struct mxlogger *mxlogger, u8 channel)
{
	struct log_msg_packet msg = {};
	struct mxmgmt_transport *mxmgmt_transport;
	struct mxlogger_channel *chan = &mxlogger->chan[channel];

	SCSC_TAG_INFO(MXMAN, "MXLOGGER Config mifram_ref: 0x%x  size:%d\n", chan->mifram_ref, chan->msz);

	if (chan->target == SCSC_MIF_ABS_TARGET_WLAN)
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(mxlogger->mx);
	else
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(mxlogger->mx);

	msg.msg = MM_MXLOGGER_CONFIG_CMD;
	msg.arg = MM_MXLOGGER_CONFIG_BASE_ADDR;
	memcpy(&msg.payload, &chan->mifram_ref, sizeof(chan->mifram_ref));

	/* Reinit the completion before sending the message over cpacketbuffer
	 * otherwise there might be a race condition
	 */
	mxlogger_wait_for_msg_reinit_completion(chan);

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING, &msg, sizeof(msg));

	SCSC_TAG_INFO(MXMAN, "MXLOGGER Config SENT\n");
	if (!mxlogger_wait_for_msg_reply(chan))
		return -ETIME;

	return 0;
}

static void mxlogger_to_shared_dram(struct mxlogger *mxlogger, u8 channel)
{
	int r;
	struct log_msg_packet msg = { .msg = MM_MXLOGGER_DIRECTION_CMD, .arg = MM_MXLOGGER_DIRECTION_DRAM };
	struct mxmgmt_transport *mxmgmt_transport;
	struct mxlogger_channel *chan = &mxlogger->chan[channel];

	SCSC_TAG_INFO(MXMAN, "MXLOGGER -- NO active observers detected. Send logs to DRAM\n");
	if (!chan->enabled) {
		SCSC_TAG_INFO(MXMAN, "Channel %s disabled\n", channel ? "WPAN" : "WLAN");
		return;
	}

	if (chan->target == SCSC_MIF_ABS_TARGET_WLAN)
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(mxlogger->mx);
	else
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(mxlogger->mx);

	r = __mxlogger_generate_sync_record(mxlogger, channel, MXLOGGER_SYN_TORAM);
	if (r)
		return;

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING, &msg, sizeof(msg));
}

static void mxlogger_to_host(struct mxlogger *mxlogger, u8 channel)
{
	int r;
	struct log_msg_packet msg = { .msg = MM_MXLOGGER_DIRECTION_CMD, .arg = MM_MXLOGGER_DIRECTION_HOST };
	struct mxmgmt_transport *mxmgmt_transport;
	struct mxlogger_channel *chan = &mxlogger->chan[channel];

	SCSC_TAG_INFO(MXMAN, "MXLOGGER -- active observers detected. Send logs to host\n");
	if (!chan->enabled) {
		SCSC_TAG_INFO(MXMAN, "Channel %s disabled\n", channel ? "WPAN" : "WLAN");
		return;
	}

	if (chan->target == SCSC_MIF_ABS_TARGET_WLAN)
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport(mxlogger->mx);
	else
		mxmgmt_transport = scsc_mx_get_mxmgmt_transport_wpan(mxlogger->mx);

	r = __mxlogger_generate_sync_record(mxlogger, channel, MXLOGGER_SYN_TOHOST);
	if (r)
		return;

	mxmgmt_transport_send(mxmgmt_transport, MMTRANS_CHAN_ID_MAXWELL_LOGGING, &msg, sizeof(msg));
}

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int mxlogger_collect_init(struct scsc_log_collector_client *collect_client)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;
	struct mxlogger_channel *chan = &mxlogger->chan[MXLOGGER_CHANNEL_WLAN];

	if (!mxlogger->initialized || !chan->enabled)
		return 0;

	mutex_lock(&mxlogger->lock);

	SCSC_TAG_INFO(MXMAN, "Started log collection WLAN\n");

	__mxlogger_generate_sync_record(mxlogger, MXLOGGER_CHANNEL_WLAN, MXLOGGER_SYN_LOGCOLLECTION);

	chan->re_enable = true;
	/**
	 * If enabled, tell FW we are stopping for collection:
	 * this way FW can dump last minute stuff and flush properly
	 * its cache
	 */
	__mxlogger_enable(mxlogger, MXLOGGER_CHANNEL_WLAN, false, MM_MXLOGGER_DISABLE_REASON_COLLECTION);

	mutex_unlock(&mxlogger->lock);

	return 0;
}

static int mxlogger_collect_init_wpan(struct scsc_log_collector_client *collect_client)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;
	struct mxlogger_channel *chan = &mxlogger->chan[MXLOGGER_CHANNEL_WPAN];

	if (!mxlogger->initialized || !chan->enabled)
		return 0;

	mutex_lock(&mxlogger->lock);

	SCSC_TAG_INFO(MXMAN, "Started log collection WPAN\n");

	__mxlogger_generate_sync_record(mxlogger, MXLOGGER_CHANNEL_WPAN, MXLOGGER_SYN_LOGCOLLECTION);

	chan->re_enable = true;
	/**
	 * If enabled, tell FW we are stopping for collection:
	 * this way FW can dump last minute stuff and flush properly
	 * its cache
	 */
	__mxlogger_enable(mxlogger, MXLOGGER_CHANNEL_WPAN, false, MM_MXLOGGER_DISABLE_REASON_COLLECTION);

	mutex_unlock(&mxlogger->lock);

	return 0;
}

static int __mxlogger_collect(struct scsc_log_collector_client *collect_client, struct mxlogger *mxlogger, size_t size,
			      enum scsc_mif_abs_target target)
{
	struct scsc_mif_abs *mif;
	void *buf;
	u8 channel;
	int ret = 0;
	int i;
	size_t sz;
	struct mxlogger_channel *chan;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		channel = MXLOGGER_CHANNEL_WLAN;
	else
		channel = MXLOGGER_CHANNEL_WPAN;

	if (mxlogger && mxlogger->mx)
		mif = scsc_mx_get_mif_abs(mxlogger->mx);
	else
		/* Return 0 as 'success' to continue the collection of other chunks */
		return 0;

	chan = &mxlogger->chan[channel];

	mutex_lock(&mxlogger->lock);

	if (mxlogger->initialized == false) {
		SCSC_TAG_ERR(MXMAN, "MXLOGGER not initialized\n");
		mutex_unlock(&mxlogger->lock);
		return 0;
	}

	switch (collect_client->type) {
	case SCSC_LOG_CHUNK_SYNC:
	case SCSC_LOG_CHUNK_SYNC_WPAN:
		i = MXLOGGER_SYNC;
		break;
	case SCSC_LOG_CHUNK_IMP:
	case SCSC_LOG_CHUNK_IMP_WPAN:
		i = MXLOGGER_IMP;
		break;
	case SCSC_LOG_RESERVED_COMMON:
	case SCSC_LOG_RESERVED_COMMON_WPAN:
		i = MXLOGGER_RESERVED_COMMON;
		break;
	case SCSC_LOG_RESERVED_BT:
	case SCSC_LOG_RESERVED_BT_WPAN:
		i = MXLOGGER_RESERVED_BT;
		break;
	case SCSC_LOG_RESERVED_WLAN:
	case SCSC_LOG_RESERVED_WLAN_WPAN:
		i = MXLOGGER_RESERVED_WLAN;
		break;
	case SCSC_LOG_RESERVED_RADIO:
	case SCSC_LOG_RESERVED_RADIO_WPAN:
		i = MXLOGGER_RESERVED_RADIO;
		break;
	case SCSC_LOG_CHUNK_MXL:
	case SCSC_LOG_CHUNK_MXL_WPAN:
		i = MXLOGGER_MXLOG;
		break;
	case SCSC_LOG_CHUNK_UDI:
	case SCSC_LOG_CHUNK_UDI_WPAN:
		i = MXLOGGER_UDI;
		break;
	default:
		SCSC_TAG_ERR(MXMAN,
			     "MXLOGGER Incorrect type. Return 'success' and continue to collect other buffers\n");
		mutex_unlock(&mxlogger->lock);
		return 0;
	}

	sz = chan->cfg->bfds[i].size;
	if (sz == 0) {
		SCSC_TAG_INFO(MXMAN, "Skipping %s buffer %s size 0\n", channel ? "WPAN" : "WLAN",
			      mxlogger_buf_name[i], sz);
		goto exit;
	}
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	buf = mif->get_mifram_ptr_region2(mif, chan->cfg->bfds[i].location);
#else
	buf = mif->get_mifram_ptr(mif, chan->cfg->bfds[i].location);
#endif
	if (!buf) {
		SCSC_TAG_ERR(MXMAN, "Memory is NULL\n");
		mutex_unlock(&mxlogger->lock);
		return -ENOMEM;
	}

	SCSC_TAG_INFO(MXMAN, "Writing %s buffer %s size: %zu\n", channel ? "WPAN" : "WLAN", mxlogger_buf_name[i],
		      sz);
	ret = scsc_log_collector_write(buf, sz, 1);
	if (ret) {
		mutex_unlock(&mxlogger->lock);
		return ret;
	}
exit:
	mutex_unlock(&mxlogger->lock);
	return 0;
}

static int mxlogger_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;

	return __mxlogger_collect(collect_client, mxlogger, size, SCSC_MIF_ABS_TARGET_WLAN);
}

static int mxlogger_collect_wpan(struct scsc_log_collector_client *collect_client, size_t size)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;

	return __mxlogger_collect(collect_client, mxlogger, size, SCSC_MIF_ABS_TARGET_WPAN);
}

static int mxlogger_collect_end(struct scsc_log_collector_client *collect_client)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;
	struct mxlogger_channel *chan = &mxlogger->chan[MXLOGGER_CHANNEL_WLAN];

	if (!mxlogger->initialized || !chan->enabled)
		return 0;

	mutex_lock(&mxlogger->lock);

	SCSC_TAG_INFO(MXMAN, "End log collection WLAN\n");

	/* Renable again if was previoulsy enabled */
	if (chan->re_enable)
		__mxlogger_enable(mxlogger, MXLOGGER_CHANNEL_WLAN, true, MM_MXLOGGER_DISABLE_REASON_STOP);

	mutex_unlock(&mxlogger->lock);
	return 0;
}

static int mxlogger_collect_end_wpan(struct scsc_log_collector_client *collect_client)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;
	struct mxlogger_channel *chan = &mxlogger->chan[MXLOGGER_CHANNEL_WPAN];

	if (!mxlogger->initialized || !chan->enabled)
		return 0;

	mutex_lock(&mxlogger->lock);

	SCSC_TAG_INFO(MXMAN, "End log collection WPAN\n");

	/* Renable again if was previoulsy enabled */
	if (chan->re_enable)
		__mxlogger_enable(mxlogger, MXLOGGER_CHANNEL_WPAN, true, MM_MXLOGGER_DISABLE_REASON_STOP);

	mutex_unlock(&mxlogger->lock);
	return 0;
}
#endif

void mxlogger_print_mapping(struct mxlogger_config_area *cfg)
{
	u8 i;

	SCSC_TAG_INFO(MXMAN, "MXLOGGER  -- Configured Buffers [%d]\n", cfg->config.num_buffers);
	for (i = 0; i < MXLOGGER_NUM_BUFFERS; i++)
		SCSC_TAG_INFO(MXMAN, "buffer %s loc: 0x%08x size: %u\n", mxlogger_buf_name[i], cfg->bfds[i].location,
			      cfg->bfds[i].size);
}

static int mxlogger_init_transport_channel_wpan(struct mxlogger *mxlogger)
{
	struct mxlogger_channel *channel = &mxlogger->chan[MXLOGGER_CHANNEL_WPAN];

	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mxlogger->mx),
						  MMTRANS_CHAN_ID_MAXWELL_LOGGING, &mxlogger_message_handler_wpan,
						  channel);
	return 0;
}

static int mxlogger_init_channel_wpan(struct mxlogger *mxlogger, uint32_t mem_sz)
{
	size_t remaining_mem;
	size_t mxl_mem_sz;
	struct miframman *miframman;
	struct mxlogger_channel *channel = &mxlogger->chan[MXLOGGER_CHANNEL_WPAN];
	struct mxlogger_config_area *cfg = channel->cfg;
	struct scsc_mif_abs *mif;

	if (!mxlogger->mx)
		return -EIO;

	mif = scsc_mx_get_mif_abs(mxlogger->mx);

	miframman = scsc_mx_get_ramman2_wpan(mxlogger->mx);
	if (!miframman)
		return -ENOMEM;

	channel->target = SCSC_MIF_ABS_TARGET_WPAN;
	channel->mxlogger = mxlogger;

	channel->mem = miframman_alloc(miframman, mem_sz, 32, MIFRAMMAN_OWNER_COMMON);
	if (!channel->mem) {
		SCSC_TAG_ERR(MXMAN, "Error allocating memory for MXLOGGER\n");
		return -ENOMEM;
	}
	channel->msz = mem_sz;
	memset(channel->mem, 0, channel->msz);

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	mif->get_mifram_ref_region2(mif, channel->mem, &channel->mifram_ref);
#else
	mif->get_mifram_ref(mif, channel->mem, &channel->mifram_ref);
#endif
	/* Initialize configuration structure */
	SCSC_TAG_INFO(MXMAN, "MXLOGGER Configuration WPAN region: 0x%x\n", (u32)channel->mifram_ref);
	cfg = (struct mxlogger_config_area *)channel->mem;

	cfg->config.magic_number = MXLOGGER_MAGIG_NUMBER;
	cfg->config.config_major = MXLOGGER_MAJOR;
	cfg->config.config_minor = MXLOGGER_MINOR;
	cfg->config.num_buffers = MXLOGGER_NUM_BUFFERS;

	/**
	 * Populate information of Fixed size buffers
	 * These are mifram-reletive references
	 */
	cfg->bfds[MXLOGGER_SYNC].location = channel->mifram_ref + offsetof(struct mxlogger_config_area, buffers_start);
	cfg->bfds[MXLOGGER_SYNC].size = MXLOGGER_SYNC_SIZE;
	/* additionally cache the va of sync_buffer */
	channel->mem_sync_buf = channel->mem + offsetof(struct mxlogger_config_area, buffers_start);

	cfg->bfds[MXLOGGER_IMP].location = cfg->bfds[MXLOGGER_IMP - 1].location + cfg->bfds[MXLOGGER_IMP - 1].size;
	cfg->bfds[MXLOGGER_IMP].size = MXLOGGER_IMP_SIZE;

	cfg->bfds[MXLOGGER_RESERVED_COMMON].location =
		cfg->bfds[MXLOGGER_RESERVED_COMMON - 1].location + cfg->bfds[MXLOGGER_RESERVED_COMMON - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_COMMON].size = MXLOGGER_RSV_COMMON_SZ_WPAN;

	cfg->bfds[MXLOGGER_RESERVED_BT].location =
		cfg->bfds[MXLOGGER_RESERVED_BT - 1].location + cfg->bfds[MXLOGGER_RESERVED_BT - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_BT].size = MXLOGGER_RSV_BT_SZ_WPAN;

	cfg->bfds[MXLOGGER_RESERVED_WLAN].location =
		cfg->bfds[MXLOGGER_RESERVED_WLAN - 1].location + cfg->bfds[MXLOGGER_RESERVED_WLAN - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_WLAN].size = MXLOGGER_RSV_WLAN_SZ_WPAN;

	cfg->bfds[MXLOGGER_RESERVED_RADIO].location =
		cfg->bfds[MXLOGGER_RESERVED_RADIO - 1].location + cfg->bfds[MXLOGGER_RESERVED_RADIO - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_RADIO].size = MXLOGGER_RSV_RADIO_SZ_WPAN;

	/* Compute buffer locations and size based on the remaining space */
	remaining_mem = channel->msz - (sizeof(struct mxlogger_config_area) + MXLOGGER_TOTAL_FIX_BUF_WPAN);

	/* Align the buffer to be cache friendly */
	mxl_mem_sz = (remaining_mem) & ~(MXLOGGER_NON_FIX_BUF_ALIGN - 1);

	SCSC_TAG_INFO(MXMAN, "remaining_mem %zu mxlogger size %zu\n", remaining_mem, mxl_mem_sz);

	cfg->bfds[MXLOGGER_MXLOG].location =
		cfg->bfds[MXLOGGER_MXLOG - 1].location + cfg->bfds[MXLOGGER_MXLOG - 1].size;
	cfg->bfds[MXLOGGER_MXLOG].size = mxl_mem_sz;

	cfg->bfds[MXLOGGER_UDI].location = cfg->bfds[MXLOGGER_UDI - 1].location + cfg->bfds[MXLOGGER_UDI - 1].size;
	cfg->bfds[MXLOGGER_UDI].size = 0;

	/* Save offset to buffers array */
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	mif->get_mifram_ref_region2(mif, cfg->bfds, &cfg->config.bfds_ref);
#else
	mif->get_mifram_ref(mif, cfg->bfds, &cfg->config.bfds_ref);
#endif

	mxlogger_print_mapping(cfg);

	channel->sync_buffer_index = 0;
	channel->cfg = cfg;
	channel->target = SCSC_MIF_ABS_TARGET_WPAN;
	channel->configured = true;

	init_completion(&channel->rings_serialized_ops);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/**
	 * Register to the collection infrastructure
	 *
	 * All of mxlogger buffers are registered here, NO matter if
	 * MXLOGGER initialization was successfull FW side.
	 *
	 * In such a case MXLOGGER-FW will simply ignore all of our following
	 * requests and we'll end up dumping empty buffers, BUT with a partially
	 * meaningful sync buffer. (since this last is written also Host side)
	 */
	mxlogger_collect_client_sync_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_sync_wpan);

	mxlogger_collect_client_imp_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_imp_wpan);

	mxlogger_collect_client_rsv_common_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_common_wpan);

	mxlogger_collect_client_rsv_bt_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_bt_wpan);

	mxlogger_collect_client_rsv_wlan_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_wlan_wpan);

	mxlogger_collect_client_rsv_radio_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_radio_wpan);

	mxlogger_collect_client_udi_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_udi_wpan);

	mxlogger_collect_client_mxl_wpan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_mxl_wpan);
#endif
	return 0;
}

static int mxlogger_init_transport_channel_wlan(struct mxlogger *mxlogger)
{
	struct mxlogger_channel *channel = &mxlogger->chan[MXLOGGER_CHANNEL_WLAN];

	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
						  MMTRANS_CHAN_ID_MAXWELL_LOGGING, &mxlogger_message_handler, channel);
	return 0;
}

static int mxlogger_init_channel_wlan(struct mxlogger *mxlogger, uint32_t mem_sz)
{
	size_t remaining_mem;
	size_t udi_mxl_mem_sz;
	struct miframman *miframman;
	struct mxlogger_channel *channel = &mxlogger->chan[MXLOGGER_CHANNEL_WLAN];
	struct mxlogger_config_area *cfg = channel->cfg;
	struct scsc_mif_abs *mif;

	if (!mxlogger->mx)
		return -EIO;

	mif = scsc_mx_get_mif_abs(mxlogger->mx);

	miframman = scsc_mx_get_ramman2(mxlogger->mx);
	if (!miframman)
		return -ENOMEM;

	channel->mem = miframman_alloc(miframman, mem_sz, 32, MIFRAMMAN_OWNER_COMMON);
	if (!channel->mem) {
		SCSC_TAG_ERR(MXMAN, "Error allocating memory for MXLOGGER\n");
		return -ENOMEM;
	}

	channel->target = SCSC_MIF_ABS_TARGET_WLAN;
	channel->mxlogger = mxlogger;

	channel->msz = mem_sz;
	memset(channel->mem, 0, channel->msz);

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	mif->get_mifram_ref_region2(mif, channel->mem, &channel->mifram_ref);
#else
	mif->get_mifram_ref(mif, channel->mem, &channel->mifram_ref);
#endif
	/* Initialize configuration structure */
	SCSC_TAG_INFO(MXMAN, "MXLOGGER Configuration WLAN region: 0x%x\n", (u32)channel->mifram_ref);
	cfg = (struct mxlogger_config_area *)channel->mem;

	cfg->config.magic_number = MXLOGGER_MAGIG_NUMBER;
	cfg->config.config_major = MXLOGGER_MAJOR;
	cfg->config.config_minor = MXLOGGER_MINOR;
	cfg->config.num_buffers = MXLOGGER_NUM_BUFFERS;

	/**
	 * Populate information of Fixed size buffers
	 * These are mifram-reletive references
	 */
	cfg->bfds[MXLOGGER_SYNC].location = channel->mifram_ref + offsetof(struct mxlogger_config_area, buffers_start);
	cfg->bfds[MXLOGGER_SYNC].size = MXLOGGER_SYNC_SIZE;
	/* additionally cache the va of sync_buffer */
	channel->mem_sync_buf = channel->mem + offsetof(struct mxlogger_config_area, buffers_start);

	cfg->bfds[MXLOGGER_IMP].location = cfg->bfds[MXLOGGER_IMP - 1].location + cfg->bfds[MXLOGGER_IMP - 1].size;
	cfg->bfds[MXLOGGER_IMP].size = mxlogger_manual_layout ? mxlogger_manual_imp : MXLOGGER_IMP_SIZE;

	cfg->bfds[MXLOGGER_RESERVED_COMMON].location =
		cfg->bfds[MXLOGGER_RESERVED_COMMON - 1].location + cfg->bfds[MXLOGGER_RESERVED_COMMON - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_COMMON].size =
		mxlogger_manual_layout ? mxlogger_manual_rsv_common : MXLOGGER_RSV_COMMON_SZ;

	cfg->bfds[MXLOGGER_RESERVED_BT].location =
		cfg->bfds[MXLOGGER_RESERVED_BT - 1].location + cfg->bfds[MXLOGGER_RESERVED_BT - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_BT].size = mxlogger_manual_layout ? mxlogger_manual_rsv_bt : MXLOGGER_RSV_BT_SZ;

	cfg->bfds[MXLOGGER_RESERVED_WLAN].location =
		cfg->bfds[MXLOGGER_RESERVED_WLAN - 1].location + cfg->bfds[MXLOGGER_RESERVED_WLAN - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_WLAN].size =
		mxlogger_manual_layout ? mxlogger_manual_rsv_wlan : MXLOGGER_RSV_WLAN_SZ;

	cfg->bfds[MXLOGGER_RESERVED_RADIO].location =
		cfg->bfds[MXLOGGER_RESERVED_RADIO - 1].location + cfg->bfds[MXLOGGER_RESERVED_RADIO - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_RADIO].size =
		mxlogger_manual_layout ? mxlogger_manual_rsv_radio : MXLOGGER_RSV_RADIO_SZ;

	/* Compute buffer locations and size based on the remaining space */
	remaining_mem = channel->msz - (sizeof(struct mxlogger_config_area) + MXLOGGER_TOTAL_FIX_BUF);

	/* Align the buffer to be cache friendly */
	udi_mxl_mem_sz = (remaining_mem >> 1) & ~(MXLOGGER_NON_FIX_BUF_ALIGN - 1);

	SCSC_TAG_INFO(MXMAN, "remaining_mem %zu udi/mxlogger size %zu\n", remaining_mem, udi_mxl_mem_sz);

	cfg->bfds[MXLOGGER_MXLOG].location =
		cfg->bfds[MXLOGGER_MXLOG - 1].location + cfg->bfds[MXLOGGER_MXLOG - 1].size;
	cfg->bfds[MXLOGGER_MXLOG].size = mxlogger_manual_layout ? mxlogger_manual_mxlog : udi_mxl_mem_sz;

	cfg->bfds[MXLOGGER_UDI].location = cfg->bfds[MXLOGGER_UDI - 1].location + cfg->bfds[MXLOGGER_UDI - 1].size;
	cfg->bfds[MXLOGGER_UDI].size = mxlogger_manual_layout ? mxlogger_manual_udi : udi_mxl_mem_sz;

	/* Save offset to buffers array */
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	mif->get_mifram_ref_region2(mif, cfg->bfds, &cfg->config.bfds_ref);
#else
	mif->get_mifram_ref(mif, cfg->bfds, &cfg->config.bfds_ref);
#endif

	mxlogger_print_mapping(cfg);

	channel->sync_buffer_index = 0;
	channel->cfg = cfg;
	channel->target = SCSC_MIF_ABS_TARGET_WLAN;
	channel->configured = true;

	init_completion(&channel->rings_serialized_ops);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/**
	 * Register to the collection infrastructure
	 *
	 * All of mxlogger buffers are registered here, NO matter if
	 * MXLOGGER initialization was successfull FW side.
	 *
	 * In such a case MXLOGGER-FW will simply ignore all of our following
	 * requests and we'll end up dumping empty buffers, BUT with a partially
	 * meaningful sync buffer. (since this last is written also Host side)
	 */
	mxlogger_collect_client_sync.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_sync);

	mxlogger_collect_client_imp.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_imp);

	mxlogger_collect_client_rsv_common.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_common);

	mxlogger_collect_client_rsv_bt.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_bt);

	mxlogger_collect_client_rsv_wlan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_wlan);

	mxlogger_collect_client_rsv_radio.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_radio);

	mxlogger_collect_client_udi.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_udi);

	mxlogger_collect_client_mxl.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_mxl);
#endif
	return 0;
}

unsigned int mxlogger_get_channel_ref(struct mxlogger *mxlogger, enum scsc_mif_abs_target target)
{
	struct mxlogger_channel *chan;
	u8 channel;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		channel = MXLOGGER_CHANNEL_WLAN;
	else
		channel = MXLOGGER_CHANNEL_WPAN;

	chan = &mxlogger->chan[channel];
	if (chan->configured == false)
		return 0;

	return chan->mifram_ref;
}

unsigned int mxlogger_get_channel_len(struct mxlogger *mxlogger, enum scsc_mif_abs_target target)
{
	struct mxlogger_channel *chan;
	u8 channel;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		channel = MXLOGGER_CHANNEL_WLAN;
	else
		channel = MXLOGGER_CHANNEL_WPAN;

	chan = &mxlogger->chan[channel];
	if (chan->configured == false)
		return 0;

	return chan->msz;
}

int mxlogger_init_channel(struct mxlogger *mxlogger, enum scsc_mif_abs_target target)
{
	if (mxlogger->configured == false) {
		SCSC_TAG_INFO(MXMAN, "Mxlogger not configured\n");
		return -EIO;
	}

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		return mxlogger_init_channel_wlan(mxlogger, MXL_POOL_SZ_WLAN);
	else
		return mxlogger_init_channel_wpan(mxlogger, MXL_POOL_SZ_WPAN);
}

int mxlogger_init_transport_channel(struct mxlogger *mxlogger, enum scsc_mif_abs_target target)
{
	if (mxlogger->configured == false) {
		SCSC_TAG_INFO(MXMAN, "Mxlogger not configured\n");
		return -EIO;
	}

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		return mxlogger_init_transport_channel_wlan(mxlogger);
	else
		return mxlogger_init_transport_channel_wpan(mxlogger);
}

static void mxlogger_deinit_channel_wlan(struct mxlogger *mxlogger)
{
	struct miframman *miframman = NULL;
	struct mxlogger_channel *channel = &mxlogger->chan[MXLOGGER_CHANNEL_WLAN];

	if (!channel || !channel->configured) {
		SCSC_TAG_INFO(MXMAN, "WLAN channel not configured\n");
		return;
	}

	channel->configured = false;

	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
						  MMTRANS_CHAN_ID_MAXWELL_LOGGING, NULL, NULL);
	miframman = scsc_mx_get_ramman2(mxlogger->mx);
	if (miframman && channel->mem) {
		miframman_free(miframman, channel->mem);
		channel->mem = NULL;
	}
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_client(&mxlogger_collect_client_sync);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_imp);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_common);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_bt);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_wlan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_radio);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_mxl);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_udi);
#endif
}

static void mxlogger_deinit_channel_wpan(struct mxlogger *mxlogger)
{
	struct miframman *miframman = NULL;
	struct mxlogger_channel *channel = &mxlogger->chan[MXLOGGER_CHANNEL_WPAN];

	if (!channel || !channel->configured) {
		SCSC_TAG_INFO(MXMAN, "WPAN channel not configured\n");
		return;
	}

	channel->configured = false;

	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mxlogger->mx),
						  MMTRANS_CHAN_ID_MAXWELL_LOGGING, NULL, NULL);

	miframman = scsc_mx_get_ramman2_wpan(mxlogger->mx);
	if (miframman && channel->mem) {
		miframman_free(miframman, channel->mem);
		channel->mem = NULL;
	}
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_client(&mxlogger_collect_client_sync_wpan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_imp_wpan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_common_wpan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_bt_wpan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_wlan_wpan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_radio_wpan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_mxl_wpan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_udi_wpan);
#endif
}

void mxlogger_deinit_channel(struct mxlogger *mxlogger, enum scsc_mif_abs_target target)
{
	if (mxlogger->configured == false) {
		SCSC_TAG_INFO(MXMAN, "Mxlogger not configured\n");
		return;
	}

	SCSC_TAG_INFO(MXMAN, "Channel %s deinit\n",
		      (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		mxlogger_deinit_channel_wlan(mxlogger);
	else
		mxlogger_deinit_channel_wpan(mxlogger);
}
/* Lock should be acquired by caller */
int mxlogger_init(struct scsc_mx *mx, struct mxlogger *mxlogger, uint32_t mem_sz)
{
	struct mxlogger_node *mn;
	uint32_t manual_total;

	MEM_LAYOUT_CHECK();

	SCSC_TAG_INFO(MXMAN, "\n");

	mxlogger->configured = false;

	if (!mxlogger_manual_layout) {
		if (mem_sz <= (sizeof(struct mxlogger_config_area) + MXLOGGER_TOTAL_FIX_BUF)) {
			SCSC_TAG_ERR(MXMAN, "Insufficient memory allocation\n");
			return -EIO;
		}
	} else {
		manual_total = mxlogger_manual_imp + mxlogger_manual_rsv_common + mxlogger_manual_rsv_bt +
			       mxlogger_manual_rsv_wlan + mxlogger_manual_rsv_radio + mxlogger_manual_mxlog +
			       mxlogger_manual_udi;

		SCSC_TAG_INFO(MXMAN, "MXLOGGER Manual layout requested %d of total %d\n", manual_total,
			      mxlogger_manual_total_mem);
		if (manual_total > mxlogger_manual_total_mem) {
			SCSC_TAG_ERR(MXMAN, "Insufficient memory allocation for FW_layout\n");
			return -EIO;
		}
	}

	mxlogger->mx = mx;
	mxlogger->enabled = false;

	mutex_init(&mxlogger->lock);
	/* To serialize chip msg handling messaging */
	mutex_init(&mxlogger->chan_lock);

	mn = kzalloc(sizeof(*mn), GFP_KERNEL);
	if (!mn)
		return -ENOMEM;

	/**
	 * Update observers status considering
	 * current value of mxlogger_forced_to_host
	 */
	update_fake_observer();

	mutex_lock(&global_lock);

	mxlogger->observers = active_global_observers;
	if (mxlogger->observers)
		SCSC_TAG_INFO(MXMAN, "Detected global %d observer[s]\n", active_global_observers);
	mutex_unlock(&global_lock);

	mn->mxl = mxlogger;
	list_add_tail(&mn->list, &mxlogger_list.list);

	mxlogger->configured = true;
	SCSC_TAG_INFO(MXMAN, "MXLOGGER Configured\n");
	return 0;
}

int mxlogger_start_channel(struct mxlogger *mxlogger, enum scsc_mif_abs_target target)
{
	u8 channel;

	if (mxlogger_disabled) {
		SCSC_TAG_WARNING(MXMAN, "MXLOGGER is disabled. Not Starting.\n");
		return -EIO;
	}

	if (!mxlogger || !mxlogger->configured) {
		SCSC_TAG_WARNING(MXMAN, "MXLOGGER is not valid or not configured.\n");
		return -EIO;
	}

	SCSC_TAG_INFO(MXMAN, "Starting mxlogger with %d observer[s]\n", mxlogger->observers);

	mutex_lock(&mxlogger->lock);
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		channel = MXLOGGER_CHANNEL_WLAN;
	else
		channel = MXLOGGER_CHANNEL_WPAN;

	if (mxlogger->chan[channel].configured == false) {
		SCSC_TAG_ERR(MXMAN, "Channel %s not configured\n", channel ? "WPAN" : "WLAN");
		mutex_unlock(&mxlogger->lock);
		return -EIO;
	}

	if (mxlogger_send_config(mxlogger, channel)) {
		SCSC_TAG_WARNING(MXMAN, "Error sending %s configuration.\n", channel ? "WPAN" : "WLAN");
		mutex_unlock(&mxlogger->lock);
		return -ENOMEM;
	}
	/* Enable channel after configuration have succeed */
	mxlogger->chan[channel].enabled = true;

	/**
	 * MXLOGGER on FW-side is at this point starting up too during
	 * WLBT chip boot and it cannot make any assumption till about
	 * the current number of observers and direction set: so, during
	 * MXLOGGER FW-side initialization, ZERO observers were registered.
	 *
	 * As a consequence on chip-boot FW-MXLOGGER defaults to:
	 *  - direction DRAM
	 *  - all rings disabled (ingressing messages discarded)
	 */
	if (!mxlogger->observers) {
		/* Enabling BEFORE communicating direction DRAM
		 * to avoid losing messages on rings.
		 */
		mxlogger_enable_channel(mxlogger, true, channel);
		mxlogger_to_shared_dram(mxlogger, channel);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_is_observer(false);
#endif
	} else {
		mxlogger_to_host(mxlogger, channel);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_is_observer(true);
#endif
		/* Enabling AFTER communicating direction HOST
		 * to avoid wrongly spilling messages into the
		 * rings early at start (like at boot).
		 */
		mxlogger_enable_channel(mxlogger, true, channel);
	}

	SCSC_TAG_INFO(MXMAN, "MXLOGGER Started.\n");
	mutex_unlock(&mxlogger->lock);

	return 0;
}

int mxlogger_stop_channel(struct mxlogger *mxlogger, enum scsc_mif_abs_target target)
{
	u8 channel;

	if (mxlogger->configured == false) {
		SCSC_TAG_INFO(MXMAN, "Mxlogger not configured\n");
		return -EIO;
	}

	SCSC_TAG_INFO(MXMAN, "Mxlogger Stopping %s\n", (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");

	mutex_lock(&mxlogger->lock);
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		channel = MXLOGGER_CHANNEL_WLAN;
	else
		channel = MXLOGGER_CHANNEL_WPAN;

	if (mxlogger->chan[channel].enabled == false) {
		SCSC_TAG_INFO(MXMAN, "Channel not enabled");
		mutex_unlock(&mxlogger->lock);
		return -EIO;
	}

	mxlogger_to_host(mxlogger, channel);
	/* Disable channel */
	mxlogger->chan[channel].enabled = false;
	mutex_unlock(&mxlogger->lock);

	return 0;
}

void mxlogger_deinit(struct scsc_mx *mx, struct mxlogger *mxlogger)
{
	struct mxlogger_node *mn, *next;
	bool match = false;
	u8 i;

	SCSC_TAG_INFO(MXMAN, "\n");

	if (!mxlogger || !mxlogger->configured) {
		SCSC_TAG_WARNING(MXMAN, "MXLOGGER is not valid or not configured.\n");
		return;
	}

	for (i = 0; i < MXLOGGER_CHANNELS; i++)
		mxlogger_to_host(mxlogger, i); /* immediately before deconfigure to get a last sync rec */

	mxlogger->configured = false;
	mxlogger->initialized = false;

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_is_observer(true);
#endif
	mxlogger_enable(mxlogger, false);

	/* Run deregistration before adquiring the mxlogger lock to avoid
	 * deadlock with log_collector.
	 */
	mxlogger_deinit_channel(mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_deinit_channel(mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mutex_lock(&mxlogger->lock);

	list_for_each_entry_safe (mn, next, &mxlogger_list.list, list) {
		if (mn->mxl == mxlogger) {
			match = true;
			list_del(&mn->list);
			kfree(mn);
		}
	}

	if (match == false)
		SCSC_TAG_ERR(MXMAN, "FATAL, no match for given scsc_mif_abs\n");

	SCSC_TAG_INFO(MXMAN, "End\n");
	mutex_unlock(&mxlogger->lock);
}

int mxlogger_register_observer(struct mxlogger *mxlogger, char *name)
{
	u8 i;

	if (mxlogger->configured == false) {
		SCSC_TAG_INFO(MXMAN, "Mxlogger not configured\n");
		return -EIO;
	}

	mutex_lock(&mxlogger->lock);

	mxlogger->observers++;

	SCSC_TAG_INFO(MXMAN, "Register observer[%d] -- %s\n", mxlogger->observers, name);

	/* Switch logs to host */
	for (i = 0; i < MXLOGGER_CHANNELS; i++)
		mxlogger_to_host(mxlogger, i);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_is_observer(true);
#endif

	mutex_unlock(&mxlogger->lock);

	return 0;
}

int mxlogger_unregister_observer(struct mxlogger *mxlogger, char *name)
{
	u8 i;

	if (mxlogger->configured == false) {
		SCSC_TAG_INFO(MXMAN, "Mxlogger not configured\n");
		return -EIO;
	}

	mutex_lock(&mxlogger->lock);

	if (mxlogger->observers == 0) {
		SCSC_TAG_INFO(MXMAN, "Incorrect number of observers\n");
		mutex_unlock(&mxlogger->lock);
		return -EIO;
	}

	mxlogger->observers--;

	SCSC_TAG_INFO(MXMAN, "UN-register observer[%d] --  %s\n", mxlogger->observers, name);

	if (mxlogger->observers == 0) {
		for (i = 0; i < MXLOGGER_CHANNELS; i++)
			mxlogger_to_shared_dram(mxlogger, i);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_is_observer(false);
#endif
	}
	mutex_unlock(&mxlogger->lock);

	return 0;
}

/* Global observer are not associated to any [mx] mxlogger instance. So it registers as
 * an observer to all the [mx] mxlogger instances.
 */
int mxlogger_register_global_observer(char *name)
{
	struct mxlogger_node *mn, *next;

	mutex_lock(&global_lock);

	active_global_observers++;

	SCSC_TAG_INFO(MXMAN, "Register global observer[%d] -- %s\n", active_global_observers, name);

	if (list_empty(&mxlogger_list.list)) {
		SCSC_TAG_INFO(MXMAN, "No instances of mxman\n");
		mutex_unlock(&global_lock);
		return -EIO;
	}

	list_for_each_entry_safe (mn, next, &mxlogger_list.list, list) {
		/* There is a mxlogger instance */
		mxlogger_register_observer(mn->mxl, name);
	}
	mutex_unlock(&global_lock);

	return 0;
}

int mxlogger_unregister_global_observer(char *name)
{
	struct mxlogger_node *mn, *next;

	mutex_lock(&global_lock);

	if (active_global_observers)
		active_global_observers--;

	SCSC_TAG_INFO(MXMAN, "UN-register global observer[%d] --  %s\n", active_global_observers, name);

	list_for_each_entry_safe (mn, next, &mxlogger_list.list, list) {
		/* There is a mxlogger instance */
		mxlogger_unregister_observer(mn->mxl, name);
	}

	mutex_unlock(&global_lock);
	return 0;
}

#if defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 12
static int mxlogger_buffer_to_index(enum scsc_log_chunk_type fw_buffer)
{
	int i;

	switch (fw_buffer) {
	case SCSC_LOG_CHUNK_SYNC:
	case SCSC_LOG_CHUNK_SYNC_WPAN:
		i = MXLOGGER_SYNC;
		break;
	case SCSC_LOG_CHUNK_IMP:
	case SCSC_LOG_CHUNK_IMP_WPAN:
		i = MXLOGGER_IMP;
		break;
	case SCSC_LOG_RESERVED_COMMON:
	case SCSC_LOG_RESERVED_COMMON_WPAN:
		i = MXLOGGER_RESERVED_COMMON;
		break;
	case SCSC_LOG_RESERVED_BT:
	case SCSC_LOG_RESERVED_BT_WPAN:
		i = MXLOGGER_RESERVED_BT;
		break;
	case SCSC_LOG_RESERVED_WLAN:
	case SCSC_LOG_RESERVED_WLAN_WPAN:
		i = MXLOGGER_RESERVED_WLAN;
		break;
	case SCSC_LOG_RESERVED_RADIO:
	case SCSC_LOG_RESERVED_RADIO_WPAN:
		i = MXLOGGER_RESERVED_RADIO;
		break;
	case SCSC_LOG_CHUNK_MXL:
	case SCSC_LOG_CHUNK_MXL_WPAN:
		i = MXLOGGER_MXLOG;
		break;
	case SCSC_LOG_CHUNK_UDI:
	case SCSC_LOG_CHUNK_UDI_WPAN:
		i = MXLOGGER_UDI;
		break;
	default:
		SCSC_TAG_ERR(MXMAN,
			     "MXLOGGER Incorrect buffer type\n");
		return -EIO;
	}

	return i;
}

size_t mxlogger_get_fw_buf_size(struct mxlogger *mxlogger, enum scsc_log_chunk_type fw_buffer, enum scsc_mif_abs_target target)
{
	struct mxlogger_channel *chan;
	size_t sz;
	u8 channel;
	int i;
	int ret = 0;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		channel = MXLOGGER_CHANNEL_WLAN;
	else
		channel = MXLOGGER_CHANNEL_WPAN;

	if (!mxlogger)
		return 0;

	chan = &mxlogger->chan[channel];

	mutex_lock(&mxlogger->lock);

	if (mxlogger->initialized == false) {
		SCSC_TAG_ERR(MXMAN, "MXLOGGER not initialized\n");
		mutex_unlock(&mxlogger->lock);
		return 0;
	}

	i = mxlogger_buffer_to_index(fw_buffer);
	if (i < 0) {
		SCSC_TAG_ERR(MXMAN, "Incorrect buffer index\n");
		mutex_unlock(&mxlogger->lock);
		return 0;
	}

	SCSC_TAG_INFO(MXMAN, "sync mxlogger before buffer read\n");
	ret = __mxlogger_generate_sync_record(mxlogger, MXLOGGER_CHANNEL_WLAN, MXLOGGER_SYN_LOGCOLLECTION);
	if (ret != 0)
		SCSC_TAG_INFO(MXMAN, "Error in syncing buffer\n");

	sz = chan->cfg->bfds[i].size;

	mutex_unlock(&mxlogger->lock);
	return sz;
}

size_t mxlogger_dump_fw_buf(struct mxlogger *mxlogger, enum scsc_log_chunk_type fw_buffer, void *buf, size_t size,
			      enum scsc_mif_abs_target target)
{
	struct scsc_mif_abs *mif;
	struct mxlogger_channel *chan;
	void *fw_buf;
	size_t sz;
	u8 channel;
	int i;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		channel = MXLOGGER_CHANNEL_WLAN;
	else
		channel = MXLOGGER_CHANNEL_WPAN;

	if (mxlogger && mxlogger->mx)
		mif = scsc_mx_get_mif_abs(mxlogger->mx);
	else
		return 0;

	chan = &mxlogger->chan[channel];

	mutex_lock(&mxlogger->lock);

	if (mxlogger->initialized == false) {
		SCSC_TAG_ERR(MXMAN, "MXLOGGER not initialized\n");
		mutex_unlock(&mxlogger->lock);
		return 0;
	}

	i = mxlogger_buffer_to_index(fw_buffer);
	if (i < 0) {
		SCSC_TAG_ERR(MXMAN, "Incorrect buffer index\n");
		goto exit;

	}

	sz = chan->cfg->bfds[i].size;
	if (sz == 0 || size > sz) {
		SCSC_TAG_INFO(MXMAN, "Skipping %s %s buffer, buffer size %d size requested %d\n", channel ? "WPAN" : "WLAN",
			      mxlogger_buf_name[i], sz, size);
		goto exit;
	}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	fw_buf = mif->get_mifram_ptr_region2(mif, chan->cfg->bfds[i].location);
#else
	fw_buf = mif->get_mifram_ptr(mif, chan->cfg->bfds[i].location);
#endif
	SCSC_TAG_INFO(MXMAN, "Writing %s buffer %s size: %zu\n", channel ? "WPAN" : "WLAN", mxlogger_buf_name[i],
				  size);

	if(fw_buf) {
		memcpy(buf, fw_buf, size);
		mutex_unlock(&mxlogger->lock);
		return size;
	}
exit:
	mutex_unlock(&mxlogger->lock);
	return 0;
}
#endif
