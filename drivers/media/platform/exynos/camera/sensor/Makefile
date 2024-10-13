obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += module_framework/i2c_dummy/ \
					module_framework/modules/ \
					module_framework/cis/ \
					module_framework/actuator/ \
					module_framework/laser_af/ \
					module_framework/flash/ \
					module_framework/aperture/ \
					module_framework/eeprom/

ifneq ($(CONFIG_CAMERA_VENDER_MCD),y)
obj-y += module_framework/ois/
endif

EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera
EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera/include
EXTRA_CFLAGS += -I$(srctree)/drivers/media/platform/exynos/camera/vendor
