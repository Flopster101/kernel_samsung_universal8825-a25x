/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIFPMUMAN_H
#define __MIFPMUMAN_H

#include <linux/mutex.h>

#ifndef CONFIG_SOC_S5E8825
#define FOREACH_PMU_MSG(PMU_MSG)                                               \
	PMU_MSG(MSG_NULL)                                                      \
	PMU_MSG(MSG_START_WLAN)                                                \
	PMU_MSG(MSG_START_WPAN)                                                \
	PMU_MSG(MSG_START_WLAN_WPAN)                                           \
	PMU_MSG(MSG_RESET_WLAN)                                                \
	PMU_MSG(MSG_RESET_WPAN)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum pmu_msg { FOREACH_PMU_MSG(GENERATE_ENUM) };

static const char *pmu_msg_string[] = { FOREACH_PMU_MSG(GENERATE_STRING) };

#define PMU_AP_MB_MSG_NULL (0x00)
#define PMU_AP_MB_MSG_START_WLAN (0x01)
#define PMU_AP_MB_MSG_START_WPAN (0x02)
#define PMU_AP_MB_MSG_START_WLAN_WPAN (0x03)
#define PMU_AP_MB_MSG_RESET_WLAN (0x04)
#define PMU_AP_MB_MSG_RESET_WPAN (0x05)
#endif /* CONFIG_SOC_S5E8825 */

/** PMU Interrupt Bit Handler prototype. */

#ifdef CONFIG_SOC_S5E8825
typedef void (*mifpmuisr_handler)(void *data, u32 cmd);
#else
typedef void (*mifpmuisr_handler)(void *data);
#endif

struct mifpmuman;
struct scsc_mif_abs;
struct mutex;

#ifdef CONFIG_SOC_S5E8825
#define MIFPMU_ERROR_WLAN	0
#define MIFPMU_ERROR_WPAN	1
#endif

int mifpmuman_init(struct mifpmuman *pmu, struct scsc_mif_abs *mif,
		   mifpmuisr_handler handler, void *irq_data);
int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch,
		      size_t ka_patch_len);
#ifdef CONFIG_SOC_S5E8825
int mifpmuman_start_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
int mifpmuman_stop_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
int mifpmuman_force_monitor_mode_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
#else
int mifpmuman_send_msg(struct mifpmuman *pmu, enum pmu_msg msg);
#endif
int mifpmuman_deinit(struct mifpmuman *pmu);

/* Inclusion in core.c treat it as opaque */
struct mifpmuman {
#ifdef CONFIG_SOC_S5E8825
	void (*pmu_irq_handler)(void *data, u32 cmd);
#else
	void (*pmu_irq_handler)(void *data);
#endif
	void *irq_data;
	bool in_use;
#ifdef CONFIG_SOC_S5E8825
	u32 last_msg;
#else
	enum pmu_msg last_msg;
#endif
	struct scsc_mif_abs *mif;
	struct mutex lock;
	struct completion msg_ack;
};

#endif
