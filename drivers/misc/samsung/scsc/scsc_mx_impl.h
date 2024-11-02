/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef _CORE_H_
#define _CORE_H_

#include <linux/firmware.h>
#include "scsc_mif_abs.h"

struct device;
struct scsc_mx;
struct mifintrbit;
struct miframman;
struct mifmboxman;
struct mxman;
struct srvman;
struct mxmgmt_transport;
struct mxproc;
struct mxfwconfig;

struct scsc_mx          *scsc_mx_create(struct scsc_mif_abs *mif);
void scsc_mx_destroy(struct scsc_mx *mx);
struct scsc_mif_abs     *scsc_mx_get_mif_abs(struct scsc_mx *mx);
struct mifintrbit       *scsc_mx_get_intrbit(struct scsc_mx *mx);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mifintrbit       *scsc_mx_get_intrbit_wpan(struct scsc_mx *mx);
struct mifpmuman	*scsc_mx_get_mifpmuman(struct scsc_mx *mx);
#endif
struct mifmuxman        *scsc_mx_get_muxman(struct scsc_mx *mx);
struct miframman        *scsc_mx_get_ramman(struct scsc_mx *mx);
struct miframman        *scsc_mx_get_ramman2(struct scsc_mx *mx);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct miframman        *scsc_mx_get_ramman_wpan(struct scsc_mx *mx);
struct miframman        *scsc_mx_get_ramman2_wpan(struct scsc_mx *mx);
#endif
struct mifabox          *scsc_mx_get_aboxram(struct scsc_mx *mx);
struct mifmboxman       *scsc_mx_get_mboxman(struct scsc_mx *mx);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mifmboxman       *scsc_mx_get_mboxman_wpan(struct scsc_mx *mx);
#endif
#ifdef CONFIG_SCSC_SMAPPER
struct mifsmapper       *scsc_mx_get_smapper(struct scsc_mx *mx);
#endif
#ifdef CONFIG_SCSC_QOS
struct mifqos           *scsc_mx_get_qos(struct scsc_mx *mx);
#endif
struct device           *scsc_mx_get_device(struct scsc_mx *mx);
struct mxman            *scsc_mx_get_mxman(struct scsc_mx *mx);
struct srvman           *scsc_mx_get_srvman(struct scsc_mx *mx);
struct mxproc           *scsc_mx_get_mxproc(struct scsc_mx *mx);
struct mxmgmt_transport *scsc_mx_get_mxmgmt_transport(struct scsc_mx *mx);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxmgmt_transport *scsc_mx_get_mxmgmt_transport_wpan(struct scsc_mx *mx);
#endif
struct gdb_transport    *scsc_mx_get_gdb_transport_wlan(struct scsc_mx *mx);
struct gdb_transport    *scsc_mx_get_gdb_transport_fxm_1(struct scsc_mx *mx);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
struct gdb_transport    *scsc_mx_get_gdb_transport_fxm_2(struct scsc_mx *mx);
#endif
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct gdb_transport *scsc_mx_get_gdb_transport_wpan(struct scsc_mx *mx);
#endif
struct mxlog            *scsc_mx_get_mxlog(struct scsc_mx *mx);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxlog            *scsc_mx_get_mxlog_wpan(struct scsc_mx *mx);
#endif
struct mxlog_transport  *scsc_mx_get_mxlog_transport(struct scsc_mx *mx);
#if IS_ENABLED(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxlog_transport  *scsc_mx_get_mxlog_transport_wpan(struct scsc_mx *mx);
#endif
struct mxlogger         *scsc_mx_get_mxlogger(struct scsc_mx *mx);
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
struct mxlogger         *scsc_mx_get_mxlogger_wpan(struct scsc_mx *mx);
#endif
struct panicmon         *scsc_mx_get_panicmon(struct scsc_mx *mx);
struct suspendmon	*scsc_mx_get_suspendmon(struct scsc_mx *mx);
struct mxfwconfig	*scsc_mx_get_mxfwconfig(struct scsc_mx *mx);
void                    scsc_mx_request_firmware_mutex_lock(struct scsc_mx *mx);
void                    scsc_mx_request_firmware_mutex_unlock(struct scsc_mx *mx);
void                    scsc_mx_request_firmware_wake_lock(struct scsc_mx *mx);
void                    scsc_mx_request_firmware_wake_unlock(struct scsc_mx *mx);

int mx140_file_get_fw(struct scsc_mx *mx, const struct firmware **firm);
int mx140_file_download_fw(struct scsc_mx *mx, void *dest, size_t dest_size, u32 *fw_image_size);
int mx140_request_file(struct scsc_mx *mx, char *path, const struct firmware **firmp);
int mx140_release_file(struct scsc_mx *mx, const struct firmware *firmp);
int mx140_basedir_file(struct scsc_mx *mx);
int mx140_exe_path(struct scsc_mx *mx, char *path, size_t len, const char *bin);
int mx140_file_select_fw(struct scsc_mx *mx, u32 suffix);
bool mx140_file_supported_hw(struct scsc_mx *mx, u32 hw_ver);
#endif
