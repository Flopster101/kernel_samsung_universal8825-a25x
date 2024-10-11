#
# Makefile for the CMUCAL drivers.
#

# Object files in subdirectories

obj-$(CONFIG_CAL_IF)			+= cmupmucal.o
cmupmucal-y				+= cal-if.o cmucal.o ra.o vclk.o pll_spec.o cmucal-debug.o
cmupmucal-$(CONFIG_PMUCAL)		+= pmucal/pmucal_system.o pmucal/pmucal_local.o pmucal/pmucal_cpu.o pmucal/pmucal_rae.o pmucal/pmucal_powermode.o
cmupmucal-$(CONFIG_PMUCAL_DBG)		+= pmucal/pmucal_dbg.o
cmupmucal-$(CONFIG_SOC_EXYNOS9820)	+= exynos9820/cal_data.o
ifndef CONFIG_SOC_EXYNOS2100_EVT0
cmupmucal-$(CONFIG_SOC_EXYNOS2100)	+= exynos2100/cal_data.o
else
cmupmucal-$(CONFIG_SOC_EXYNOS2100_EVT0)	+= exynos2100_evt0/cal_data.o
endif
ifndef CONFIG_SOC_S5E9925_EVT0
cmupmucal-$(CONFIG_SOC_S5E9925)		+= s5e9925/cal_data.o
else
cmupmucal-$(CONFIG_SOC_S5E9925_EVT0)		+= s5e9925_evt0/cal_data.o
endif
cmupmucal-$(CONFIG_SOC_S5E8825)		+= s5e8825/cal_data.o
cmupmucal-$(CONFIG_ACPM_DVFS)		+= acpm_dvfs.o fvmap.o
cmupmucal-$(CONFIG_EXYNOS_DEBUG_FREQ)        += exynos_debug_freq.o

cmupmucal-$(CONFIG_CP_PMUCAL) += pmucal/pmucal_cp.o
cmupmucal-$(CONFIG_GNSS_PMUCAL) += pmucal/pmucal_gnss.o
cmupmucal-$(CONFIG_CHUB_PMUCAL) += pmucal/pmucal_chub.o
obj-$(CONFIG_CMU_EWF)			+= cmu_ewf.o
