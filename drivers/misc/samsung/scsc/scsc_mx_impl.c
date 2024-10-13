/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "scsc_mx_impl.h"
#include "mifintrbit.h"
#include "miframman.h"
#include "mifmboxman.h"
#ifdef CONFIG_SCSC_SMAPPER
#include "mifsmapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include "mifqos.h"
#endif
#include "mifproc.h"
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#include "mxman_if.h"
#else
#include "mxman.h"
#endif
#include "mxproc.h"
#include "mxsyserr.h"
#include "srvman.h"
#include "mxmgmt_transport.h"
#include "gdb_transport.h"
#include "mxlog.h"
#include "mxlogger.h"
#include "panicmon.h"
#include "mxlog_transport.h"
#include "suspendmon.h"
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
#include "mifpmuman.h"
#endif

#include "scsc/api/bt_audio.h"
#include "mxfwconfig.h"
#ifdef CONFIG_SCSC_WLBTD
#include "scsc_wlbtd.h"
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif

struct scsc_mx {
	struct scsc_mif_abs     *mif_abs;
	struct mifintrbit       intr;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mifintrbit       intr_wpan;
#endif
	struct miframman        ram;
	struct miframman        ram2;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct miframman        ram_wpan;
	struct miframman        ram2_wpan;
#endif
	struct mifmboxman       mbox;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mifmboxman       mbox_wpan;
#endif
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mifpmuman	pmu;
#endif
	struct mifabox          mifabox;
#ifdef CONFIG_SCSC_SMAPPER
	struct mifsmapper	smapper;
#endif
#ifdef CONFIG_SCSC_QOS
	struct mifqos		qos;
#endif
	struct mifproc          proc;
	struct mxman            mxman;
	struct srvman           srvman;
	struct mxmgmt_transport mxmgmt_transport;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mxmgmt_transport mxmgmt_transport_wpan;
#endif
	struct gdb_transport    gdb_transport_wlan;
	struct gdb_transport    gdb_transport_fxm_1;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct gdb_transport    gdb_transport_fxm_2;
#endif
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct gdb_transport    gdb_transport_wpan;
#endif
	int                     users;
	struct mxlog            mxlog;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mxlog            mxlog_wpan;
#endif
	struct mxlogger         mxlogger;
	struct panicmon         panicmon;
	struct mxlog_transport  mxlog_transport;
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	struct mxlog_transport  mxlog_transport_wpan;
#endif
	struct suspendmon	suspendmon;
	struct mxfwconfig	mxfwconfig;
	struct mutex            scsc_mx_read_mutex;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock   scsc_mx_wl_request_firmware;
#else
	struct wake_lock        scsc_mx_wl_request_firmware;
#endif
};


struct scsc_mx *scsc_mx_create(struct scsc_mif_abs *mif)
{
	struct scsc_mx *mx;

	mx = kzalloc(sizeof(*mx), GFP_KERNEL);
	if (!mx)
		return NULL;

	mx->mif_abs = mif;

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	mifintrbit_init(&mx->intr, mif, SCSC_MIF_ABS_TARGET_WLAN);
	mifintrbit_init(&mx->intr_wpan, mif, SCSC_MIF_ABS_TARGET_WPAN);
#else
	mifintrbit_init(&mx->intr, mif);
#endif

	mifmboxman_init(&mx->mbox);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	mifmboxman_init(&mx->mbox_wpan);
#endif
	suspendmon_init(&mx->suspendmon, mx);
	mxman_init(&mx->mxman, mx);
	srvman_init(&mx->srvman, mx);
	mifproc_create_proc_dir(mx->mif_abs);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_init();
#endif
	mx_syserr_init();
	mutex_init(&mx->scsc_mx_read_mutex);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	wake_lock_init(&mx->scsc_mx_wl_request_firmware, WAKE_LOCK_SUSPEND, "scsc_mx_request_firmware");
#else
	wake_lock_init(NULL, &mx->scsc_mx_wl_request_firmware.ws, "scsc_mx_request_firmware");
#endif
	SCSC_TAG_DEBUG(MXMAN, "Hurray Maxwell is here with %p\n", mx);
	return mx;
}

void scsc_mx_destroy(struct scsc_mx *mx)
{
	SCSC_TAG_DEBUG(MXMAN, "\n");
	BUG_ON(mx == NULL);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	mifintrbit_deinit(scsc_mx_get_intrbit(mx), SCSC_MIF_ABS_TARGET_WLAN);
	mifintrbit_deinit(scsc_mx_get_intrbit_wpan(mx), SCSC_MIF_ABS_TARGET_WPAN);
#else
	mifintrbit_deinit(&mx->intr);
#endif
	mifmboxman_deinit(scsc_mx_get_mboxman(mx));
#ifdef CONFIG_SCSC_SMAPPER
	mifsmapper_deinit(scsc_mx_get_smapper(mx));
#endif
	suspendmon_deinit(scsc_mx_get_suspendmon(mx));
	mifproc_remove_proc_dir();
	srvman_deinit(&mx->srvman);
	mxman_deinit(&mx->mxman);
	wake_lock_destroy(&mx->scsc_mx_wl_request_firmware);
	mutex_destroy(&mx->scsc_mx_read_mutex);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_deinit();
#endif
	kfree(mx);
	SCSC_TAG_DEBUG(MXMAN, "OK\n");
}

struct scsc_mif_abs *scsc_mx_get_mif_abs(struct scsc_mx *mx)
{
	return mx->mif_abs;
}

struct mifintrbit *scsc_mx_get_intrbit(struct scsc_mx *mx)
{
	return &mx->intr;
}

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mifintrbit *scsc_mx_get_intrbit_wpan(struct scsc_mx *mx)
{
	return &mx->intr_wpan;
}

struct mifpmuman *scsc_mx_get_mifpmuman(struct scsc_mx *mx)
{
	return &mx->pmu;
}
#endif

struct miframman *scsc_mx_get_ramman(struct scsc_mx *mx)
{
	return &mx->ram;
}

struct miframman *scsc_mx_get_ramman2(struct scsc_mx *mx)
{
	return &mx->ram2;
}

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct miframman *scsc_mx_get_ramman2_wpan(struct scsc_mx *mx)
{
	return &mx->ram2_wpan;
}

struct miframman *scsc_mx_get_ramman_wpan(struct scsc_mx *mx)
{
	return &mx->ram_wpan;
}
#endif

struct mifabox *scsc_mx_get_aboxram(struct scsc_mx *mx)
{
	return &mx->mifabox;
}

struct mifmboxman *scsc_mx_get_mboxman(struct scsc_mx *mx)
{
	return &mx->mbox;
}

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mifmboxman *scsc_mx_get_mboxman_wpan(struct scsc_mx *mx)
{
	return &mx->mbox_wpan;
}
#endif

#ifdef CONFIG_SCSC_SMAPPER
struct mifsmapper *scsc_mx_get_smapper(struct scsc_mx *mx)
{
	return &mx->smapper;
}
#endif

#ifdef CONFIG_SCSC_QOS
struct mifqos *scsc_mx_get_qos(struct scsc_mx *mx)
{
	return &mx->qos;
}
#endif

struct device *scsc_mx_get_device(struct scsc_mx *mx)
{
	return mx->mif_abs->get_mif_device(mx->mif_abs);
}
EXPORT_SYMBOL_GPL(scsc_mx_get_device); /* TODO: export a top-level API for this */

struct mxman *scsc_mx_get_mxman(struct scsc_mx *mx)
{
	return &mx->mxman;
}

struct srvman *scsc_mx_get_srvman(struct scsc_mx *mx)
{
	return &mx->srvman;
}

struct mxmgmt_transport *scsc_mx_get_mxmgmt_transport(struct scsc_mx *mx)
{
	return &mx->mxmgmt_transport;
}

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxmgmt_transport *scsc_mx_get_mxmgmt_transport_wpan(struct scsc_mx *mx)
{
	return &mx->mxmgmt_transport_wpan;
}
#endif

struct gdb_transport *scsc_mx_get_gdb_transport_wlan(struct scsc_mx *mx)
{
	return &mx->gdb_transport_wlan;
}

struct gdb_transport *scsc_mx_get_gdb_transport_fxm_1(struct scsc_mx *mx)
{
	return &mx->gdb_transport_fxm_1;
}

#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
struct gdb_transport *scsc_mx_get_gdb_transport_fxm_2(struct scsc_mx *mx)
{
	return &mx->gdb_transport_fxm_2;
}
#endif

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct gdb_transport *scsc_mx_get_gdb_transport_wpan(struct scsc_mx *mx)
{
	return &mx->gdb_transport_wpan;
}
#endif

struct mxlog *scsc_mx_get_mxlog(struct scsc_mx *mx)
{
	return &mx->mxlog;
}

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxlog *scsc_mx_get_mxlog_wpan(struct scsc_mx *mx)
{
	return &mx->mxlog_wpan;
}

#endif

struct panicmon *scsc_mx_get_panicmon(struct scsc_mx *mx)
{
	return &mx->panicmon;
}

struct mxlog_transport *scsc_mx_get_mxlog_transport(struct scsc_mx *mx)
{
	return &mx->mxlog_transport;
}

#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxlog_transport  *scsc_mx_get_mxlog_transport_wpan(struct scsc_mx *mx)
{
	return &mx->mxlog_transport_wpan;
}
#endif

struct mxlogger *scsc_mx_get_mxlogger(struct scsc_mx *mx)
{
	return &mx->mxlogger;
}

struct suspendmon *scsc_mx_get_suspendmon(struct scsc_mx *mx)
{
	return &mx->suspendmon;
}

struct mxfwconfig *scsc_mx_get_mxfwconfig(struct scsc_mx *mx)
{
	return &mx->mxfwconfig;
}

void scsc_mx_request_firmware_mutex_lock(struct scsc_mx *mx)
{
	mutex_lock(&mx->scsc_mx_read_mutex);
}
void scsc_mx_request_firmware_wake_lock(struct scsc_mx *mx)
{
	wake_lock(&mx->scsc_mx_wl_request_firmware);
}
void scsc_mx_request_firmware_mutex_unlock(struct scsc_mx *mx)
{
	mutex_unlock(&mx->scsc_mx_read_mutex);
}
void scsc_mx_request_firmware_wake_unlock(struct scsc_mx *mx)
{
	wake_unlock(&mx->scsc_mx_wl_request_firmware);
}
