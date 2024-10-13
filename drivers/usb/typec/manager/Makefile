#
# Makefile for usb typec manager drivers
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)	+= usb_typec_manager.o
usb_typec_manager-y				:= usb_typec_manager_notifier.o
usb_typec_manager-$(CONFIG_USB_HW_PARAM)	+= usb_typec_manager_hwparam.o

obj-$(CONFIG_IF_CB_MANAGER)			+= if_cb_manager.o

