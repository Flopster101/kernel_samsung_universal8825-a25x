#
# Makefile for the Linux kernel device drivers.
#
# Sep 2018, Binse Park <h22yap@gmail.com>
# Rewritten to use lists instead of if-statements.
#

FLAG=1

ifneq ($(CONFIG_KPERFMON), y)
FLAG=0
endif

ifeq ($(FLAG), 1)
ifeq ($(CONFIG_KPERFMON_BUILD), y)
	obj-y		+= kperfmon.o
else
	obj-m		+= kperfmon.o
endif
endif
