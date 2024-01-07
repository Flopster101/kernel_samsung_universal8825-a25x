#
# Makefile for nanohub
#
TARGET_SOC ?= s5e8825

obj-$(CONFIG_NANOHUB) += nanohub.o
nanohub-y := chub.o chub_bootup.o chub_dbg.o chub_log.o chub_exynos.o
nanohub-y += chub_ipc_if.o ipc_chub.o ipc_chub_ap.o ipc_common.o ipc_hw.o
nanohub-$(CONFIG_SENSOR_DRV) += main.o comms.o


nanohub-y += chub_shub.o