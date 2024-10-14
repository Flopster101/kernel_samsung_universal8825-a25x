#
# Makefile for ACPM.
#

obj-$(CONFIG_EXYNOS_ACPM)    += exynos_acpm.o 
exynos_acpm-$(CONFIG_EXYNOS_ACPM)    += acpm.o acpm_ipc.o
exynos_acpm-$(CONFIG_EXYNOS_MFD_I3C)	+= acpm_mfd_i3c.o
exynos_acpm-$(CONFIG_EXYNOS_MFD_SPMI)	+= acpm_mfd_spmi.o
obj-$(CONFIG_EXYNOS_ACPM_S2D)	+= acpm_s2d.o
obj-$(CONFIG_EXYNOS_ACPM_PLGDBG)	+= plugin_dbg.o
