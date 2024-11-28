# Makefile for the Himax touchscreen drivers.
TARGET = himax_ts_hx831xx

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)),)
	$(TARGET)-objs += himax_debug.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)),)
	$(TARGET)-objs += himax_inspection.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_INCELL_CORE)),)
	$(TARGET)-objs += himax_ic_incell_core.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_ONCELL_CORE)),)
	$(TARGET)-objs += himax_ic_oncell_core.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)),)
	$(TARGET)-objs += himax_ic_HX83102.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191)),)
	$(TARGET)-objs += himax_ic_HX83191.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)),)
	$(TARGET)-objs += himax_ic_HX83112.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83111)),)
	$(TARGET)-objs += himax_ic_HX83111.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83103)),)
	$(TARGET)-objs += himax_ic_HX83103.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121)),)
	$(TARGET)-objs += himax_ic_HX83121.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_COMMON)),)
	$(TARGET)-objs += himax_common.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_I2C)),)
	$(TARGET)-objs += himax_platform.o
endif
ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_SPI)),)
	$(TARGET)-objs += himax_platform_SPI.o
endif

obj-$(CONFIG_TOUCHSCREEN_HIMAX_COMMON) += $(TARGET).o

ccflags-y += -Wformat

