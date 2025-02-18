# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

ifdef CONFIG_SOC_S5E9925
dpu-dir := exynos_p1
cal-dir := $(dpu-dir)/cal_pm
endif

ifdef CONFIG_SOC_S5E8825
dpu-dir := exynos_p2
cal-dir := $(dpu-dir)/cal_py
endif

ifdef CONFIG_UML
dpu-dir := exynos_p1
cal-dir := $(dpu-dir)/cal_pm
endif

ifdef CONFIG_DRM_MCD_HDR
hdr-dir := mcd_cal
else
hdr-dir := $(dpu-dir)
endif

subdir-ccflags-y			:= -I$(srctree)/$(src)			\
					   -I$(srctree)/$(src)/$(dpu-dir)	\
					   -I$(srctree)/$(src)/include		\
					   -I$(srctree)/$(src)/cal_common	\
					   -I$(srctree)/$(src)/$(cal-dir)

subdir-ccflags-$(CONFIG_DRM_MCD_COMMON)	+= -I$(srctree)/$(src)/mcd_cal

exynos-drm-y := exynos_drm_drv.o  exynos_drm_fb.o exynos_drm_format.o		\
		exynos_drm_crtc.o exynos_drm_plane.o exynos_drm_connector.o	\
		exynos_drm_debug.o exynos_drm_gem.o exynos_drm_migov.o		\
		exynos_drm_tui.o exynos_drm_partial.o				\
		exynos_drm_recovery.o						\
		$(dpu-dir)/exynos_drm_hibernation.o				\
		$(dpu-dir)/exynos_drm_dqe.o		\
		$(hdr-dir)/exynos_drm_hdr.o		\
		$(dpu-dir)/exynos_drm_freq_hop.o

exynos-drm-$(CONFIG_DRM_MCD_COMMON) += mcd_cal/mcd_drm_decon.o mcd_cal/mcd_drm_dsim.o mcd_cal/mcd_drm_helper.o

exynos-drm-$(CONFIG_DRM_FBDEV_EMULATION) += exynos_drm_fbdev.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DECON)	+= $(dpu-dir)/exynos_drm_decon.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DPP)	+= $(dpu-dir)/exynos_drm_dpp.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DSI)	+= $(dpu-dir)/exynos_drm_dsim.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_WB)	+= $(dpu-dir)/exynos_drm_writeback.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DP)	+= $(dpu-dir)/exynos_drm_dp.o		\
					   $(dpu-dir)/exynos_drm_dp_edid.o	\
					   $(dpu-dir)/exynos_drm_dp_hdcp13.o	\
					   $(dpu-dir)/exynos_drm_dp_hdcp22_if.o	\
					   $(cal-dir)/dp_reg.o
exynos-drm-$(CONFIG_EXYNOS_DMA_DSIMFC)	+= $(dpu-dir)/exynos_drm_dsimfc.o	\
					   $(cal-dir)/dsimfc_reg.o

exynos-drm-y				+= $(cal-dir)/decon_reg.o		\
					   $(cal-dir)/dsim_reg.o		\
					   $(cal-dir)/dpp_reg.o			\
					   $(cal-dir)/dqe_reg.o			\
					   $(cal-dir)/hdr_reg.o			\

exynos-drm-$(CONFIG_EXYNOS_BTS)		+= $(dpu-dir)/exynos_drm_bts.o

obj-$(CONFIG_DRM_SAMSUNG_DPU)		+= exynos-drm.o
ifneq ($(filter y m,$(CONFIG_DRM_PANEL_MCD_COMMON)),)
subdir-ccflags-y += -I$(srctree)/drivers/gpu/drm/samsung/panel
obj-$(CONFIG_DRM_PANEL_MCD_COMMON)	+= panel/
else
obj-$(CONFIG_DRM_PANEL_SAMSUNG_COMMON)	+= panel/
endif
ifneq ($(filter y m,$(CONFIG_SEC_KUNIT)),)
subdir-ccflags-y += \
    -Wframe-larger-than=8192 \
    -Wno-unused-result \
    -Wno-unused-variable \
    -Wno-unused-function \
    -Wno-missing-braces \
    -Wno-implicit-fallthrough \
    -Wno-format
endif

obj-$(CONFIG_SEC_DISPLAYPORT_EXT_FUNCTION)	+= dp_ext_func/
