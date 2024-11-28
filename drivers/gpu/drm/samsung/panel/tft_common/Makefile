# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

usdm-panel-tft-common-y := \
	tft_function.o \
	tft_common.o

obj-$(CONFIG_USDM_PANEL_TFT_COMMON) += usdm-panel-tft-common.o

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wunused-variable \
	-Wunused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wno-format


mcd-panel-hx83102e_gta4xls_00-y += hx83102e_gta4xls_00.o
obj-$(CONFIG_USDM_PANEL_HX83102E_GTA4XLS_00) += mcd-panel-hx83102e_gta4xls_00.o
