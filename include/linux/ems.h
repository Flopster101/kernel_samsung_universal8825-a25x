/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_EMS_H
#define _LINUX_EMS_H

#include <linux/sched.h>
#include <linux/pm_qos.h>

/*
 * Sysbusy
 */
enum sysbusy_state {
	SYSBUSY_STATE0 = 0,
	SYSBUSY_STATE1,
	SYSBUSY_STATE2,
	SYSBUSY_STATE3,
	NUM_OF_SYSBUSY_STATE,
};

#define SYSBUSY_CHECK_BOOST	(0)
#define SYSBUSY_STATE_CHANGE	(1)

/*
 * EMStune
 */
struct emstune_mode_request {
	struct dev_pm_qos_request dev_req;
	char *func;
	unsigned int line;
};

#define EMSTUNE_GAME_MODE		(3)

#define BITS_OF_BOOST_TYPE		(3)
#define EMSTUNE_BOOST_TYPE_EXTREME	(1 << (BITS_OF_BOOST_TYPE - 1)) /* Last bit in boost level rage */
#define EMSTUNE_BOOST_TYPE_MASK		((1 << BITS_OF_BOOST_TYPE) - 1)
#define EMSTUNE_BOOST_TYPE(type)	(type & EMSTUNE_BOOST_TYPE_MASK)

#define BEGIN_OF_MODE_TYPE		BITS_OF_BOOST_TYPE
#define EMSTUNE_MODE_TYPE_GAME		(1 << (BEGIN_OF_MODE_TYPE + EMSTUNE_GAME_MODE))
#define EMSTUNE_MODE_TYPE_MASK		((-1) << BEGIN_OF_MODE_TYPE)

#define EMSTUNE_MODE_TYPE(type)		((type & EMSTUNE_MODE_TYPE_MASK) >> BEGIN_OF_MODE_TYPE)

#if IS_ENABLED(CONFIG_SCHED_EMS)
extern int emstune_get_cur_mode(void);
extern int emstune_get_cur_level(void);

#define emstune_add_request(req)	do {				\
	__emstune_add_request(req, (char *)__func__, __LINE__);	\
} while(0);
extern void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line);
extern void emstune_update_request(struct emstune_mode_request *req, s32 new_value);
extern void emstune_remove_request(struct emstune_mode_request *req);
extern void emstune_boost(struct emstune_mode_request *req, int enable);

extern int emstune_register_notifier(struct notifier_block *nb);
extern int emstune_unregister_notifier(struct notifier_block *nb);

extern int emstune_cpu_dsu_table_index(void *_set);

extern int ecs_request_register(char *name, const struct cpumask *mask);
extern int ecs_request_unregister(char *name);
extern int ecs_request(char *name, const struct cpumask *mask);

extern void et_init_dsu_table(unsigned long *freq_table, unsigned int *volt_table, int size);
extern void et_register_dsu_constraint(int cpu, void *p, int size);

extern int sysbusy_register_notifier(struct notifier_block *nb);
extern int sysbusy_unregister_notifier(struct notifier_block *nb);
extern void halo_register_periodic_irq(int irq_num, const char *name, struct cpumask *cpus, void (*fn)(u64 *cnt, ktime_t *time));
extern void halo_update_periodic_irq(int irq_num, u64 cnt, ktime_t last_update_ns, ktime_t period_ns);
extern void halo_unregister_periodic_irq(int irq_num);

extern void register_mhdvfs_dsufreq_callback(void (*callback)(int ratio, int period_ns));
extern void register_mhdvfs_miffreq_callback(void (*callback)(int ratio));
extern void register_mhdvfs_cpufreq_callback(void (*callback)(int *ratio));
#else
static inline void stt_acquire_gmc_boost(int step, int cnt) { }
static inline void stt_release_gmc_boost(void) { }
static inline int emstune_get_cur_mode(void) { return -1; }
static inline int emstune_get_cur_level(void) { return -1; }

#define emstune_add_request(req)	do { } while(0);
static inline void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line) { }
static inline void emstune_update_request(struct emstune_mode_request *req, s32 new_value) { }
static inline void emstune_remove_request(struct emstune_mode_request *req) { }
static inline void emstune_boost(struct emstune_mode_request *req, int enable) { }

static inline int emstune_register_notifier(struct notifier_block *nb) { return 0; }
static inline int emstune_unregister_notifier(struct notifier_block *nb) { return 0; }

static inline int emstune_cpu_dsu_table_index(void *_set) { return 0; }

static inline int ecs_request_register(char *name, const struct cpumask *mask) { return 0; }
static inline int ecs_request_unregister(char *name) { return 0; }
static inline int ecs_request(char *name, const struct cpumask *mask) { return 0; }

static inline void et_init_dsu_table(unsigned long *freq_table, unsigned int *volt_table, int size) { }
static inline void et_register_dsu_constraint(int cpu, void *p, int size) { }

static inline int sysbusy_register_notifier(struct notifier_block *nb) { return 0; };
static inline int sysbusy_unregister_notifier(struct notifier_block *nb) { return 0; };

static inline void halo_register_periodic_irq(int irq_num, const char *name, struct cpumask *cpus, void (*fn)(u64 *cnt, ktime_t *time)) { };
static inline void halo_update_periodic_irq(int irq_num, u64 cnt, ktime_t last_update_ns, ktime_t period_ns) { };
static inline void halo_unregister_periodic_irq(int irq_num) { };

static inline void register_mhdvfs_dsufreq_callback(void (*callback)(int ratio)) { };
static inline void register_mhdvfs_miffreq_callback(void (*callback)(int ratio)) { };
static inline void register_mhdvfs_cpufreq_callback(void (*callback)(int *ratio)) { };
#endif

#endif	/* ENDIF _LINUX_EMS_H */
