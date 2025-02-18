#obj-$(CONFIG_EXYNOS_DSP)	+= dsp/

#
# Makefile for the npu device driver
#

ccflags-y += -I$(srctree)/drivers/vision/vision-core/include -Wformat
ccflags-y += -I$(srctree)/drivers/vision/npu/core -Wformat
ccflags-y += -I$(srctree)/drivers/vision/npu/core/include -Wformat
ifeq ($(CONFIG_DSP_USE_VS4L),y)
ccflags-y += -I$(srctree)/drivers/vision/npu/core/dl -Wformat
endif

ifeq ($(CONFIG_NPU_HARDWARE),y)
ccflags-y += -I$(srctree)/drivers/vision/npu/core/interface/hardware
else # CONFIG NPU_LOOPBACK
ccflags-y += -I$(srctree)/drivers/vision/npu/core/interface/loopback
endif

ifeq ($(CONFIG_NPU_STM),y)
ccflags-y += -I$(srctree)/drivers/hwtracing/coresight
endif

ifeq ($(CONFIG_NPU_USE_MBR),y)
ccflags-y += -I$(srctree)/drivers/vision/dsp -Wformat
endif

ifeq ($(CONFIG_NPU_BUILD_BUILTIN),y)
obj-$(CONFIG_VISION_CORE)	+= vision-core/vision-dev.o
obj-$(CONFIG_VISION_CORE)	+= vision-core/vision-compat-ioctl32.o
obj-$(CONFIG_VISION_CORE)	+= vision-core/vision-ioctl.o
obj-$(CONFIG_VISION_CORE)	+= vision-core/vision-buffer.o

obj-y					+= npu/core/npu-util-autosleepthr.o
obj-y					+= npu/core/npu-util-liststatemgr.o
obj-y					+= npu/core/npu-util-msgidgen.o
obj-y					+= npu/core/npu-util-regs.o
obj-y					+= npu/core/npu-util-llq.o
obj-y					+= npu/core/npu-util-common.o
obj-y					+= npu/core/npu-llc.o
obj-y					+= npu/core/npu-log.o
obj-y					+= npu/core/npu-ver.o
obj-y					+= npu/core/npu-debug.o
obj-y					+= npu/core/npu-device.o
obj-y					+= npu/core/npu-core.o
obj-y					+= npu/core/npu-clock.o
obj-y					+= npu/core/npu-vertex.o
obj-y					+= npu/core/npu-binary.o
obj-y					+= npu/core/npu-queue.o
obj-y					+= npu/core/npu-qos.o
obj-y					+= npu/core/npu-system.o
obj-y					+= npu/core/npu-memory.o
obj-y					+= npu/core/npu-protodrv.o
obj-y					+= npu/core/npu-session.o
obj-y					+= npu/core/npu-fw-test-handler.o
obj-y					+= npu/core/npu-queue.o
obj-y					+= npu/core/npu-if-session-protodrv.o
obj-y					+= npu/core/npu-if-protodrv-mbox2.o
obj-y					+= npu/core/npu-sessionmgr.o
obj-y					+= npu/core/npu-config.o
obj-$(CONFIG_NPU_USE_HW_DEVICE)		+= npu/core/npu-hw-device.o

obj-$(CONFIG_NPU_GOLDEN_MATCH)	        += npu/core/npu-golden.o
obj-y	        += npu/core/npu-util-statekeeper.o
obj-$(CONFIG_NPU_GOLDEN_MATCH)	        += npu/core/npu-util-comparator.o

obj-$(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS)	        += npu/core/npu-util-memdump.o

obj-$(CONFIG_NPU_SCHEDULER_GOVERNOR)	+= npu/core/npu-scheduler.o\
					npu/core/ npu-scheduler-governor.o

obj-$(CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS)	+= npu/core/npu-scheduler-governor-simple-exynos.o

obj-$(CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE)	+= npu/core/npu-scheduler-governor-exynos-interactive.o

obj-$(CONFIG_NPU_GOVERNOR_USERSPACE)	+= npu/core/npu-scheduler-governor-userspace.o

obj-$(CONFIG_NPU_USE_SPROFILER)		+= npu/core/npu-profile.o

obj-$(CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER)	+= npu/core/generated/npu-ver-info.o

ifeq ($(CONFIG_NPU_INTERFACE),y)
obj-$(CONFIG_NPU_HARDWARE)		+= npu/core/interface/npu-interface.o
obj-$(CONFIG_NPU_HARDWARE)		+= npu/core/interface/mailbox_ipc.o
obj-$(CONFIG_NPU_LOOPBACK)		+= npu/core/loopback/npu-interface.o
obj-$(CONFIG_NPU_LOOPBACK)		+= npu/core/loopback/mailbox_ipc.o
endif

obj-$(CONFIG_NPU_STM)			+= npu/core/npu-stm.o

ifeq ($(CONFIG_DSP_USE_VS4L),y)
obj-y					+= npu/core/dsp-binary.o
obj-y					+= npu/core/dsp-kernel.o
obj-y					+= npu/core/dsp-util.o
obj-y					+= npu/core/dsp-dhcp.o

obj-y					+= npu/core/dl/dsp-common.o
obj-y					+= npu/core/dl/dsp-dl-engine.o
obj-y					+= npu/core/dl/dsp-dl-linker.o
obj-y					+= npu/core/dl/dsp-dl-out-manager.o
obj-y					+= npu/core/dl/dsp-elf-loader.o
obj-y					+= npu/core/dl/dsp-gpt-manager.o
obj-y					+= npu/core/dl/dsp-hash.o
obj-y					+= npu/core/dl/dsp-lib-manager.o
obj-y					+= npu/core/dl/dsp-list.o
obj-y					+= npu/core/dl/dsp-llstack.o
obj-y					+= npu/core/dl/dsp-pm-manager.o
obj-y					+= npu/core/dl/dsp-rule-reader.o
obj-y					+= npu/core/dl/dsp-string-tree.o
obj-y					+= npu/core/dl/dsp-sxml.o
obj-y					+= npu/core/dl/dsp-tlsf-allocator.o
obj-y					+= npu/core/dl/dsp-xml-parser.o
endif
else
npu-objs				+= vision-core/vision-dev.o
ifeq ($(CONFIG_NPU_COMPAT_IOCTL), y)
npu-objs				+= vision-core/vision-compat-ioctl32.o
endif
npu-objs				+= vision-core/vision-ioctl.o
npu-objs				+= vision-core/vision-buffer.o
npu-objs				+= vision-core/vision-log.o

npu-objs				+= npu/core/npu-util-autosleepthr.o
npu-objs				+= npu/core/npu-util-liststatemgr.o
npu-objs				+= npu/core/npu-util-msgidgen.o
npu-objs				+= npu/core/npu-util-regs.o
npu-objs				+= npu/core/npu-util-llq.o
npu-objs				+= npu/core/npu-core.o
npu-objs				+= npu/core/npu-clock.o
npu-objs				+= npu/core/npu-util-common.o
npu-objs				+= npu/core/npu-llc.o
npu-objs				+= npu/core/npu-log.o
npu-objs				+= npu/core/npu-ver.o
npu-objs				+= npu/core/npu-debug.o
npu-objs				+= npu/core/npu-device.o
npu-objs				+= npu/core/npu-vertex.o
npu-objs				+= npu/core/npu-binary.o
npu-objs				+= npu/core/npu-queue.o
npu-objs				+= npu/core/npu-qos.o
npu-objs				+= npu/core/npu-system.o
npu-objs				+= npu/core/npu-memory.o
npu-objs				+= npu/core/npu-protodrv.o
npu-objs				+= npu/core/npu-session.o
npu-objs				+= npu/core/npu-fw-test-handler.o
npu-objs				+= npu/core/npu-queue.o
npu-objs				+= npu/core/npu-if-session-protodrv.o
npu-objs				+= npu/core/npu-if-protodrv-mbox2.o
npu-objs				+= npu/core/npu-sessionmgr.o
npu-objs				+= npu/core/npu-config.o
ifeq ($(CONFIG_NPU_USE_HW_DEVICE),y)
npu-objs				+= npu/core/npu-hw-device.o
endif
ifeq ($(CONFIG_NPU_INTERFACE),y)
ifeq ($(CONFIG_NPU_HARDWARE),y)
npu-objs				+= npu/core/interface/hardware/npu-interface.o
npu-objs				+= npu/core/interface/hardware/mailbox_ipc.o
else # CONFIG_NPU_LOOPBACK
npu-objs				+= npu/core/interface/loopback/npu-interface.o
npu-objs				+= npu/core/interface/loopback/mailbox_ipc.o
endif
endif

ifeq ($(CONFIG_NPU_GOLDEN_MATCH),y)
npu-objs			        += npu/core/npu-golden.o
npu-objs				+= npu/core/npu-util-comparator.o
endif
npu-objs				+= npu/core/npu-util-statekeeper.o

ifeq ($(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS),y)
npu-objs				+= npu/core/npu-util-memdump.o
endif

ifeq ($(CONFIG_NPU_SCHEDULER_GOVERNOR),y)
npu-objs				+= npu/core/npu-scheduler.o
npu-objs				+= npu/core/npu-scheduler-governor.o
ifeq ($(CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS),y)
npu-objs				+= npu/core/npu-scheduler-governor-simple-exynos.o
endif
ifeq ($(CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE),y)
npu-objs				+= npu/core/npu-scheduler-governor-exynos-interactive.o
endif
ifeq ($(CONFIG_NPU_GOVERNOR_USERSPACE),y)
npu-objs				+= npu/core/npu-scheduler-governor-userspace.o
endif
endif

ifeq ($(CONFIG_NPU_USE_SPROFILER),y)
npu-objs				+= npu/core/npu-profile.o
endif

ifeq ($(CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER),y)
npu-objs				+= npu/core/generated/npu-ver-info.o
endif

ifeq ($(CONFIG_NPU_STM),y)
npu-objs				+= npu/core/npu-stm.o
endif

ifeq ($(CONFIG_DSP_USE_VS4L),y)
npu-objs				+= npu/core/dsp-binary.o
npu-objs				+= npu/core/dsp-kernel.o
npu-objs				+= npu/core/dsp-util.o
npu-objs				+= npu/core/dsp-dhcp.o

npu-objs				+= npu/core/dl/dsp-common.o
npu-objs				+= npu/core/dl/dsp-dl-engine.o
npu-objs				+= npu/core/dl/dsp-dl-linker.o
npu-objs				+= npu/core/dl/dsp-dl-out-manager.o
npu-objs				+= npu/core/dl/dsp-elf-loader.o
npu-objs				+= npu/core/dl/dsp-gpt-manager.o
npu-objs				+= npu/core/dl/dsp-hash.o
npu-objs				+= npu/core/dl/dsp-lib-manager.o
npu-objs				+= npu/core/dl/dsp-list.o
npu-objs				+= npu/core/dl/dsp-llstack.o
npu-objs				+= npu/core/dl/dsp-pm-manager.o
npu-objs				+= npu/core/dl/dsp-rule-reader.o
npu-objs				+= npu/core/dl/dsp-string-tree.o
npu-objs				+= npu/core/dl/dsp-sxml.o
npu-objs				+= npu/core/dl/dsp-tlsf-allocator.o
npu-objs				+= npu/core/dl/dsp-xml-parser.o
endif

obj-$(CONFIG_EXYNOS_NPU)		+= npu.o
endif
