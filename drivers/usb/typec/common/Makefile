#
# Makefile for usbpd common drivers
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_PDIC_NOTIFIER)		+= pdic_notifier_module.o
pdic_notifier_module-y		:= pdic_notifier.o pdic_sysfs.o pdic_core.o pdic_misc.o pdic_param.o
pdic_notifier_module-$(CONFIG_PDIC_POLICY) += pdic_policy.o
