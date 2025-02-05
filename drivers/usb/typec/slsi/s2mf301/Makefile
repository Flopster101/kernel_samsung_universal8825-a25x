#
# Makefile for usbpd devices
#
subdir-ccflags-y := -Wformat

pdic_s2mf301-y		:= s2mf301-usbpd.o
pdic_s2mf301-$(CONFIG_S2MF301_TYPEC_WATER) += s2mf301-water.o
obj-$(CONFIG_PDIC_S2MF301)	+= pdic_s2mf301.o
