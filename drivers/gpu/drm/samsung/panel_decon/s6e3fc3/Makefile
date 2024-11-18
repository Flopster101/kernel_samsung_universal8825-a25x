# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wno-unused-variable \
	-Wno-unused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wno-format


mcd-panel-s6e3fc3_a33x-y += s6e3fc3.o s6e3fc3_a33x.o
mcd-panel-s6e3fc3_a33x-$(CONFIG_EXTEND_LIVE_CLOCK) += s6e3fc3_aod.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FC3) += mcd-panel-s6e3fc3_a33x.o