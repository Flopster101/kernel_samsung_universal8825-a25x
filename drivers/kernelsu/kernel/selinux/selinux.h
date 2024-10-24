#ifndef __KSU_H_SELINUX
#define __KSU_H_SELINUX

#include "linux/types.h"
#include "linux/version.h"

void setup_selinux(const char *);

void setenforce(bool);

bool getenforce();

bool is_ksu_domain();

bool is_zygote(void *cred);

void apply_kernelsu_rules();

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
u32 ksu_get_zygote_sid(void);

u32 ksu_get_current_sid(void);
#endif

u32 ksu_get_devpts_sid();

#endif
