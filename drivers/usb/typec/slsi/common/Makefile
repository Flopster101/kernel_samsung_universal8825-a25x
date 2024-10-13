#
# Makefile for usbpd devices
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_PDIC_SLSI_NON_MCU)		+= s2m_pdic_notifier_module.o

s2m_pdic_notifier_module-y			:= s2m_pdic_notifier.o usbpd.o usbpd_cc.o usbpd_policy.o usbpd_manager.o
