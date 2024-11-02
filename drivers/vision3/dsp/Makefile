#
# Makefile for the dsp common driver
#

ccflags-y := -Wformat
subdir-ccflags-y := -Wformat

ccflags-y += -I$(srctree)/drivers/vision3/dsp
ccflags-y += -I$(srctree)/drivers/vision3/dsp/core
ccflags-y += -I$(srctree)/drivers/vision3/dsp/platform
ccflags-y += -I$(srctree)/drivers/vision3/dsp/utils

ccflags-y += -I$(srctree)/drivers/vision3/dsp/hardware
ccflags-y += -I$(srctree)/drivers/vision3/dsp/hardware/include
ccflags-y += -I$(srctree)/drivers/vision3/dsp/hardware/common
ccflags-y += -I$(srctree)/drivers/vision3/dsp/hardware/dummy

ccflags-y += -I$(srctree)/drivers/vision3/dsp/debug

dsp-objs			:=

dsp-objs			+= dsp-driver.o
dsp-objs			+= dsp-npu.o

dsp-objs			+= core/dsp-device.o
dsp-objs			+= core/dsp-context.o
dsp-objs			+= core/dsp-core.o
dsp-objs			+= core/dsp-ioctl.o
ifeq ($(CONFIG_COMPAT),y)
dsp-objs			+= core/dsp-compat-ioctl.o
endif
dsp-objs			+= core/dsp-kernel.o
dsp-objs			+= core/dsp-graph.o

dsp-objs			+= utils/dsp-task.o
dsp-objs			+= utils/dsp-util.o
dsp-objs			+= utils/dsp-time.o
dsp-objs			+= utils/dsp-binary.o
dsp-objs			+= utils/dsp-log.o

dsp-objs			+= dl/dsp-common.o
dsp-objs			+= dl/dsp-dl-engine.o
dsp-objs			+= dl/dsp-dl-linker.o
dsp-objs			+= dl/dsp-dl-out-manager.o
dsp-objs			+= dl/dsp-elf-loader.o
dsp-objs			+= dl/dsp-gpt-manager.o
dsp-objs			+= dl/dsp-hash.o
dsp-objs			+= dl/dsp-lib-manager.o
dsp-objs			+= dl/dsp-list.o
dsp-objs			+= dl/dsp-llstack.o
dsp-objs			+= dl/dsp-pm-manager.o
dsp-objs			+= dl/dsp-rule-reader.o
dsp-objs			+= dl/dsp-string-tree.o
dsp-objs			+= dl/dsp-sxml.o
dsp-objs			+= dl/dsp-tlsf-allocator.o
dsp-objs			+= dl/dsp-xml-parser.o

dsp-objs			+= hardware/dsp-hardware.o

dsp-objs			+= hardware/common/dsp-hw-common-init.o
dsp-objs			+= hardware/common/dsp-hw-common-system.o
dsp-objs			+= hardware/common/dsp-hw-common-pm.o
dsp-objs			+= hardware/common/dsp-hw-common-clk.o
dsp-objs			+= hardware/common/dsp-hw-common-bus.o
dsp-objs			+= hardware/common/dsp-hw-common-llc.o
dsp-objs			+= hardware/common/dsp-hw-common-interface.o
dsp-objs			+= hardware/common/dsp-hw-common-ctrl.o
dsp-objs			+= hardware/common/dsp-hw-common-memory.o
dsp-objs			+= hardware/common/dsp-hw-common-mailbox.o
dsp-objs			+= hardware/common/dsp-hw-common-governor.o
dsp-objs			+= hardware/common/dsp-hw-common-dump.o
dsp-objs			+= hardware/common/dsp-hw-common-log.o

dsp-objs			+= hardware/dummy/dsp-hw-dummy-llc.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-interface.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-memory.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-imgloader.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-sysevent.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-memlogger.o

ifeq ($(CONFIG_EXYNOS_DSP_HW_N1),y)
dsp-objs			+= hardware/N1/dsp-hw-n1-system.o
dsp-objs			+= hardware/N1/dsp-hw-n1-pm.o
dsp-objs			+= hardware/N1/dsp-hw-n1-clk.o
dsp-objs			+= hardware/N1/dsp-hw-n1-bus.o
dsp-objs			+= hardware/N1/dsp-hw-n1-interface.o
dsp-objs			+= hardware/N1/dsp-hw-n1-ctrl.o
dsp-objs			+= hardware/N1/dsp-hw-n1-memory.o
dsp-objs			+= hardware/N1/dsp-hw-n1-mailbox.o
dsp-objs			+= hardware/N1/dsp-hw-n1-governor.o
dsp-objs			+= hardware/N1/dsp-hw-n1-dump.o
dsp-objs			+= hardware/N1/dsp-hw-n1-log.o
endif

ifeq ($(CONFIG_EXYNOS_DSP_HW_N3),y)
dsp-objs			+= hardware/N3/dsp-hw-n3-system.o
dsp-objs			+= hardware/N3/dsp-hw-n3-pm.o
dsp-objs			+= hardware/N3/dsp-hw-n3-clk.o
dsp-objs			+= hardware/N3/dsp-hw-n3-bus.o
dsp-objs			+= hardware/N3/dsp-hw-n3-interface.o
dsp-objs			+= hardware/N3/dsp-hw-n3-ctrl.o
dsp-objs			+= hardware/N3/dsp-hw-n3-memory.o
dsp-objs			+= hardware/N3/dsp-hw-n3-mailbox.o
dsp-objs			+= hardware/N3/dsp-hw-n3-governor.o
dsp-objs			+= hardware/N3/dsp-hw-n3-dump.o
dsp-objs			+= hardware/N3/dsp-hw-n3-log.o
endif

ifeq ($(CONFIG_EXYNOS_DSP_HW_O1),y)
dsp-objs			+= hardware/O1/dsp-hw-o1-system.o
dsp-objs			+= hardware/O1/dsp-hw-o1-pm.o
dsp-objs			+= hardware/O1/dsp-hw-o1-clk.o
dsp-objs			+= hardware/O1/dsp-hw-o1-bus.o
dsp-objs			+= hardware/O1/dsp-hw-o1-llc.o
dsp-objs			+= hardware/O1/dsp-hw-o1-interface.o
dsp-objs			+= hardware/O1/dsp-hw-o1-ctrl.o
dsp-objs			+= hardware/O1/dsp-hw-o1-memory.o
dsp-objs			+= hardware/O1/dsp-hw-o1-mailbox.o
dsp-objs			+= hardware/O1/dsp-hw-o1-governor.o
dsp-objs			+= hardware/O1/dsp-hw-o1-dump.o
dsp-objs			+= hardware/O1/dsp-hw-o1-log.o
endif

ifeq ($(CONFIG_EXYNOS_DSP_HW_O3),y)
dsp-objs			+= hardware/O3/dsp-hw-o3-system.o
dsp-objs			+= hardware/O3/dsp-hw-o3-pm.o
dsp-objs			+= hardware/O3/dsp-hw-o3-clk.o
dsp-objs			+= hardware/O3/dsp-hw-o3-bus.o
dsp-objs			+= hardware/O3/dsp-hw-o3-interface.o
dsp-objs			+= hardware/O3/dsp-hw-o3-ctrl.o
dsp-objs			+= hardware/O3/dsp-hw-o3-memory.o
dsp-objs			+= hardware/O3/dsp-hw-o3-mailbox.o
dsp-objs			+= hardware/O3/dsp-hw-o3-governor.o
dsp-objs			+= hardware/O3/dsp-hw-o3-dump.o
dsp-objs			+= hardware/O3/dsp-hw-o3-log.o
endif

ifeq ($(CONFIG_EXYNOS_DSP_HW_P0),y)
dsp-objs			+= hardware/P0/dsp-hw-p0-system.o
dsp-objs			+= hardware/P0/dsp-hw-p0-pm.o
dsp-objs			+= hardware/P0/dsp-hw-p0-clk.o
dsp-objs			+= hardware/P0/dsp-hw-p0-bus.o
dsp-objs			+= hardware/P0/dsp-hw-p0-llc.o
dsp-objs			+= hardware/P0/dsp-hw-p0-interface.o
dsp-objs			+= hardware/P0/dsp-hw-p0-ctrl.o
dsp-objs			+= hardware/P0/dsp-hw-p0-memory.o
dsp-objs			+= hardware/P0/dsp-hw-p0-mailbox.o
dsp-objs			+= hardware/P0/dsp-hw-p0-governor.o
dsp-objs			+= hardware/P0/dsp-hw-p0-dump.o
dsp-objs			+= hardware/P0/dsp-hw-p0-log.o
endif

ifeq ($(CONFIG_EXYNOS_IMGLOADER),m)
dsp-objs			+= hardware/common/dsp-hw-common-imgloader.o
ifeq ($(CONFIG_EXYNOS_DSP_HW_O1),y)
dsp-objs			+= hardware/O1/dsp-hw-o1-imgloader.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_O3),y)
dsp-objs			+= hardware/O3/dsp-hw-o3-imgloader.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_P0),y)
dsp-objs			+= hardware/P0/dsp-hw-p0-imgloader.o
endif
endif

ifeq ($(CONFIG_EXYNOS_SYSTEM_EVENT),m)
dsp-objs			+= hardware/common/dsp-hw-common-sysevent.o
ifeq ($(CONFIG_EXYNOS_DSP_HW_O1),y)
dsp-objs			+= hardware/O1/dsp-hw-o1-sysevent.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_O3),y)
dsp-objs			+= hardware/O3/dsp-hw-o3-sysevent.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_P0),y)
dsp-objs			+= hardware/P0/dsp-hw-p0-sysevent.o
endif
endif

ifeq ($(CONFIG_EXYNOS_MEMORY_LOGGER),m)
dsp-objs			+= hardware/common/dsp-hw-common-memlogger.o
ifeq ($(CONFIG_EXYNOS_DSP_HW_O1),y)
dsp-objs			+= hardware/O1/dsp-hw-o1-memlogger.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_O3),y)
dsp-objs			+= hardware/O3/dsp-hw-o3-memlogger.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_P0),y)
dsp-objs			+= hardware/P0/dsp-hw-p0-memlogger.o
endif
endif

dsp-objs			+= debug/dsp-debug-core.o
dsp-objs			+= debug/hardware/common/dsp-hw-common-debug.o
ifeq ($(CONFIG_EXYNOS_DSP_HW_N1),y)
dsp-objs			+= debug/hardware/N1/dsp-hw-n1-debug.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_N3),y)
dsp-objs			+= debug/hardware/N3/dsp-hw-n3-debug.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_O1),y)
dsp-objs			+= debug/hardware/O1/dsp-hw-o1-debug.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_O3),y)
dsp-objs			+= debug/hardware/O3/dsp-hw-o3-debug.o
endif
ifeq ($(CONFIG_EXYNOS_DSP_HW_P0),y)
dsp-objs			+= debug/hardware/P0/dsp-hw-p0-debug.o
endif

obj-$(CONFIG_EXYNOS_DSP)	+= dsp.o
