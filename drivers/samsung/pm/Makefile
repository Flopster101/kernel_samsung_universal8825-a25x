# drivers/samsung/pm/Makefile
#
# Copyright (c) 2010-2021 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Samsung Mobile Power Management Debugging Support
obj-$(CONFIG_SEC_PM_DEBUG)		+= sec_pm_debug.o
obj-$(CONFIG_SEC_PM_THERMISTOR)		+= sec_thermistor.o
obj-$(CONFIG_SEC_PM_CPUFREQ)		+= sec_pm_cpufreq.o
obj-$(CONFIG_SEC_WAKEUP_CPU_ALLOCATOR)		+= sec_wakeup_cpu_allocator.o
obj-$(CONFIG_RTC_AUTO_PWRON)	+= sec_pon_alarm.o

ccflags-y := -Wformat
