#ifndef __KSU_H_SUCOMPAT
#define __KSU_H_SUCOMPAT

#ifdef CONFIG_KSU_SUSFS_SUS_SU
void ksu_susfs_disable_sus_su(void);
void ksu_susfs_enable_sus_su(void);
#endif

#endif
