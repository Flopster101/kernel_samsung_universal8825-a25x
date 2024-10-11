#
# Makefile for the vibrator driver ICs with vibrator driver
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_DC_VIBRATOR)        += dc_vibrator.o
ifeq ($(CONFIG_SEC_KUNIT), y)
GCOV_PROFILE_dc_vibrator.o := $(CONFIG_SEC_KUNIT)
endif

