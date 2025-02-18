#@file Makefile
#@brief Makefile for Next Generation Knox Security Monitor driver
#Copyright (c) 2019, Samsung Electronics Corporation. All rights reserved.
#
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 2 and
#only version 2 as published by the Free Software Foundation.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.


ccflags-y := -Wformat
ccflags-y += -I$(srctree)/$(src)

ifeq ($(CONFIG_NGKSM), y)
    $(warning [NGKSM] CONFIG_NGKSM Enabled)
else
    $(warning [NGKSM] CONFIG_NGKSM Not Enabled)
endif

ifeq ($(CONFIG_NGKPAD), y)
    $(warning [NGKSM] CONFIG_NGKPAD Enabled)
else
    $(warning [NGKSM] CONFIG_NGKPAD Not Enabled)
endif

obj-$(CONFIG_NGKSM) += ngksm_main.o
obj-$(CONFIG_NGKSM) += ngksm_kernel_api.o
obj-$(CONFIG_NGKSM) += ngksm_rate_limit.o
obj-$(CONFIG_NGKSM) += ngksm_netlink.o

ifeq ($(CONFIG_NGKPAD),y)
    ccflags-y += -DNGK_PAD
    obj-$(CONFIG_NGKPAD) += ngk_hypervisor_detector.o
endif

$(warning [NGKSM] TARGET_BUILD_VARIANT $(TARGET_BUILD_VARIANT))

ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
    $(warning [NGKSM] NGK_DEBUG)
    ccflags-y += -DNGK_DEBUG
endif

ifeq ($(CONFIG_UH), y)
    $(warning [NGKSM] CONFIG_UH Enabled)
    ccflags-y += -DCONFIG_NGKSM_UH=true
endif
