# SPDX-License-Identifier: GPL-2.0
ccflags-y += -I$(src)			# needed for trace events

obj-y					+= ion/

obj-$(CONFIG_ASHMEM)			+= ashmem.o
obj-$(CONFIG_DEBUG_KINFO)	+= debug_kinfo.o

obj-$(CONFIG_SAMSUNG_LMKD_DEBUG)		+= lmkd_debug.o
obj-$(CONFIG_SAMSUNG_FREECESS)		+= freecess.o
obj-$(CONFIG_SAMSUNG_FREECESS)		+= freecess_pkg.o

include $(srctree)/drivers/staging/android/Makefile.variant1
