# SPDX-License-Identifier: GPL-2.0
# Makefile of gnssif

ccflags-y += -Wformat
ccflags-y += -Wformat-zero-length

obj-$(CONFIG_EXYNOS_GNSS_IF) += gnss_mbox.o

obj-$(CONFIG_EXYNOS_GNSS_IF) += gnssif.o
gnssif-y += gnss_main.o gnss_io_device.o gnss_keplerctl_device.o
gnssif-y += gnss_link_device_shmem.o gnss_link_device_memory.o
gnssif-y += pmu-gnss.o gnss_utils.o
