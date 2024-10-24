#ifndef __KSU_H_KSU_CORE
#define __KSU_H_KSU_CORE

#include <linux/init.h>

void __init ksu_core_init(void);
void ksu_core_exit(void);

#ifdef CONFIG_KSU_SUSFS
bool susfs_is_allow_su(void);
void escape_to_root(void);
void try_umount(const char *mnt, bool check_mnt, int flags);
#endif

#endif
