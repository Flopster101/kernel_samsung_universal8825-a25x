# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_EXYNOS_CHIPID_V2)	+= exynos-chipid_v2.o

obj-$(CONFIG_EXYNOS_ASV)	+= exynos-asv.o
obj-$(CONFIG_EXYNOS_ASV_ARM)	+= exynos5422-asv.o

obj-$(CONFIG_EXYNOS_CHIPID)	+= exynos-chipid.o
obj-$(CONFIG_EXYNOS_PMU)	+= exynos-pmu.o
obj-$(CONFIG_EXYNOS_PMU_IF)	+= exynos-pmu-if.o
obj-$(CONFIG_EXYNOS_FLEXPMU_DBG)            += exynos-flexpmu-dbg.o

obj-$(CONFIG_EXYNOS_PMU_ARM_DRIVERS)	+= exynos3250-pmu.o exynos4-pmu.o \
					exynos5250-pmu.o exynos5420-pmu.o
obj-$(CONFIG_EXYNOS_PM_DOMAINS) += pm_domains.o
obj-$(CONFIG_EXYNOS_REGULATOR_COUPLER) += exynos-regulator-coupler.o

obj-$(CONFIG_SAMSUNG_PM_CHECK)	+= s3c-pm-check.o
obj-$(CONFIG_SAMSUNG_PM_DEBUG)	+= s3c-pm-debug.o

obj-$(CONFIG_EXYNOS_PD)		+= exynos-pd/

# CAL
obj-$(CONFIG_CAL_IF)            += cal-if/
#obj-$(CONFIG_CAL_IF)	+= exynos-powermode.o

# ACPM
obj-$(CONFIG_EXYNOS_ACPM)	+= acpm/

# USI_V2
obj-$(CONFIG_USI_V2) += usi_v2.o

# Exynos Memlogger
obj-$(CONFIG_EXYNOS_MEMORY_LOGGER)      += memlogger.o

# Exynos Subsystem
obj-$(CONFIG_EXYNOS_SYSTEM_EVENT)       += sysevent.o sysevent_notif.o

# Exynos Image Loader
obj-$(CONFIG_EXYNOS_IMGLOADER)       += imgloader.o

# DEBUG
obj-$(CONFIG_EXYNOS_DEBUG)      += debug/

# ECT
obj-$(CONFIG_ECT)               += ect_parser/
obj-$(CONFIG_EXYNOS_PM) 	+= exynos-pm/

# DVFS MANAGER
obj-$(CONFIG_EXYNOS_DVFS_MANAGER)       += dm/
obj-$(CONFIG_EXYNOS_MCINFO)		+= exynos-mcinfo.o

# CPU Hotplug
obj-$(CONFIG_EXYNOS_CPUHP)	+= exynos-cpuhp.o

# Exynos CPU Performance
obj-$(CONFIG_EXYNOS_PERF)       += xperf/

# CPU Power Management
obj-$(CONFIG_EXYNOS_CPUPM)	+= exynos-cpupm.o

# Exynos User Frequency & Cstate Control
obj-$(CONFIG_EXYNOS_UFCC)	+= exynos-ufcc.o

# AFM
obj-$(CONFIG_EXYNOS_AFM)        += exynos-afm.o

# CPIF
obj-$(CONFIG_EXYNOS_MODEM_IF) += cpif/

# GNSSIF
obj-$(CONFIG_EXYNOS_GNSS_IF) += gnssif/

# EXYNOS_PM_QOS
obj-$(CONFIG_EXYNOS_PM_QOS)	+= pm_qos/

# SCI
obj-$(CONFIG_EXYNOS_SCI)		+= exynos_sci.o
exynos_sci-$(CONFIG_EXYNOS_SCI)		+= exynos-sci.o
exynos_sci-$(CONFIG_EXYNOS_SCI_DBG)	+= exynos-sci_dbg.o

# EXYNOS_BCM
obj-$(CONFIG_EXYNOS_BCM_DBG)    += exynos-bcm.o
obj-$(CONFIG_EXYNOS_BCM_DBG_DUMP)       += exynos-bcm_dbg-dump.o
exynos-bcm-y			+= exynos-bcm_dbg.o exynos-bcm_dbg-dt.o
exynos-bcm-$(CONFIG_SOC_S5E9925) += exynos-bcm_dbg-9925.o
exynos-bcm-$(CONFIG_SOC_S5E8825) += exynos-bcm_dbg-8825.o

# Exynos Samsung Secure Platform
obj-$(CONFIG_EXYNOS_SSP)	+= exynos-ssp.o

# Exynos CryptoManager
obj-$(CONFIG_EXYNOS_CRYPTOMANAGER)	+= exynos-cm.o

# Exynos EL2
obj-$(CONFIG_EXYNOS_EL2)	+= exynos-el2.o

# Exynos S2MPU
obj-$(CONFIG_EXYNOS_S2MPU)	+= exynos-s2mpu.o

# SDM
obj-$(CONFIG_EXYNOS_SDM)	+= exynos-sdm.o

# Exynos Secure Log
obj-$(CONFIG_EXYNOS_SECURE_LOG) += exynos-seclog.o

obj-$(CONFIG_EXYNOS_TZASC_FAIL_DETECTOR) += exynos-tzasc.o
obj-$(CONFIG_EXYNOS_PPMPU_FAIL_DETECTOR) += exynos-ppmpu.o

obj-$(CONFIG_EXYNOS_SEH)        += exynos-seh.o
# secmem
obj-$(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)       += secmem.o

obj-$(CONFIG_EXYNOS_HDCP2)  += exynos-hdcp/

# Multi IP Governor
obj-$(CONFIG_EXYNOS_MIGOV)		+= exynos-migov.o
obj-$(CONFIG_EXYNOS_GPU_PROFILER)	+= exynos-gpu-profiler.o
obj-$(CONFIG_EXYNOS_CPU_PROFILER)	+= exynos-cpu-profiler.o
obj-$(CONFIG_EXYNOS_MIF_PROFILER)	+= exynos-mif-profiler.o

# Exynos WoW
obj-$(CONFIG_EXYNOS_WOW)       += exynos-wow.o
