#
# Crypto TEE Driver
#

obj-$(CONFIG_MZ_TEE_DRIVER) += mz_tee_driver.o

mz_tee_driver-$(CONFIG_MZ_USE_QSEECOM) += mz_tee_driver_qc.o
mz_tee_driver-$(CONFIG_MZ_USE_TZDEV) += mz_tee_driver_teegris.o

ccflags-$(CONFIG_MZ_TEE_DRIVER) += -I$(srctree)/drivers/security/samsung/mz
ccflags-$(CONFIG_MZ_TEE_DRIVER) += -I$(srctree)/drivers/misc
ccflags-$(CONFIG_MZ_USE_TZDEV) += -I$(srctree)/drivers/misc/tzdev/$(CONFIG_TEEGRIS_VERSION_STRING)/include

subdir-ccflags-y := -Wformat

ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
	ccflags-y += -DMZ_DEBUG -DDEBUG
endif
