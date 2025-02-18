obj-$(CONFIG_SCHED_EMS) += ems.o


ifeq ($(AMIGO_VER), )
ifdef CONFIG_SOC_S5E8535
	ccflags-y += -DAMIGO_BUILD_VER=3
	AMIGO_VER=3
else
	ccflags-y += -DAMIGO_BUILD_VER=4
	AMIGO_VER=4
endif
else
	ccflags-y += -DAMIGO_BUILD_VER=$(AMIGO_VER)
endif

ems-y += main.o
ems-y += core.o
ems-y += energy_table.o
ems-y += multi_load.o
ems-y += ontime.o
ems-y += sparing.o
ems-y += ecs.o
ems-y += ecs_dynamic.o
ems-y += freqboost.o
ems-y += hook.o
ems-y += sysbusy.o
ems-y += frt.o
ems-y += profile.o
ems-y += freq_variant.o
ems-y += ego.o
ems-y += cpufreq.o
ems-y += balance.o
ems-$ += frt.o
ems-y += gsc.o
ems-$(CONFIG_SCHED_EMS_TUNE) += tune.o
ems-$(CONFIG_CPU_IDLE_GOV_HALO) += halo.o
ems-$(CONFIG_MHDVFS) += mhdvfs.o
