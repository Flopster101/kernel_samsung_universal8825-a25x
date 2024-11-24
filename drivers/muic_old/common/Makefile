#
# Makefile for muic devices
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_LEGACY_MUIC_NOTIFIER)     += common_muic-legacy.o
common_muic-legacy-y   := muic_notifier.o muic_param.o
common_muic-legacy-$(CONFIG_LEGACY_MUIC_SYSFS) += muic_sysfs.o
common_muic-legacy-$(CONFIG_LEGACY_MUIC_CORE) += muic_core.o
obj-$(CONFIG_LEGACY_MUIC_MANAGER) += muic_manager.o
