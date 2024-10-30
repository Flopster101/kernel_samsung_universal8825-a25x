#
# Makefile for the stui Module
#

ccflags-y += -I$(srctree)/drivers/misc/tui/
ccflags-y += -Idrivers/misc/tzdev/include/tzdev
ccflags-y += -DUSE_TEE_CLIENT_API

tuihw-inf-objs := stui_inf.o

tuihw-objs := main.o
tuihw-objs += stui_core.o

ifeq ($(CONFIG_SOC_S5E9925),y)
ccflags-y += -DCONFIG_SOC_S5E9925
ccflags-y += -I$(srctree)/drivers/misc/tui/platforms/exynos9925
tuihw-objs += platforms/exynos9925/stui_hal_display.o
tuihw-objs += platforms/exynos9925/stui_hal_touch.o
endif

ifeq ($(CONFIG_SOC_S5E8825),y)
ccflags-y += -DCONFIG_SOC_S5E8825
ccflags-y += -I$(srctree)/drivers/misc/tui/platforms/exynos8825
tuihw-objs += platforms/exynos8825/stui_hal_display.o
tuihw-objs += platforms/exynos8825/stui_hal_touch.o
endif

ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
tuihw-objs += iwd_agent.o

ccflags-y += -I$(srctree)/drivers/misc
ccflags-y += -I$(srctree)/drivers/misc/tzdev/
ccflags-y += -DCONFIG_SAMSUNG_TUI_LOWLEVEL
endif

obj-$(CONFIG_SAMSUNG_TUI) += tuihw.o tuihw-inf.o
