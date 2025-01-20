TARGET = stm_ts
TARGET_SPI = stm_ts_spi

$(TARGET)-objs := stm_i2c.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o
ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
  $(TARGET_SPI)-objs := stm_spi.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_ioctl.o
else
  $(TARGET_SPI)-objs := stm_spi.o stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_ioctl.o stm_vendor.o
endif

obj-$(CONFIG_TOUCHSCREEN_STM) += $(TARGET).o
obj-$(CONFIG_TOUCHSCREEN_STM_SPI) += $(TARGET_SPI).o

ccflags-y += -Wformat

