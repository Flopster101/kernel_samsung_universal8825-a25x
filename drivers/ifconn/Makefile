#
# Makefile for usb typec manager drivers
#

obj-$(CONFIG_IFCONN_NOTIFIER_MODULE)		+= ifconn_notifier_module.o
ifconn_notifier_module-$(CONFIG_IFCONN_NOTIFIER) += ifconn_notifier.o
ifconn_notifier_module-$(CONFIG_IFCONN_MANAGER) += ifconn_manager.o
