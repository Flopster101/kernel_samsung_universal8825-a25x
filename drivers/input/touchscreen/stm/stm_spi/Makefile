TARGET = stm_ts_spi

ifeq ($(CONFIG_ARCH_WAIPIO), y)
  ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
    $(TARGET)-objs := stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o
    obj-$(CONFIG_TOUCHSCREEN_STM_SPI) += $(TARGET).o
  else
    $(TARGET)-objs := stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_vendor.o
    obj-$(CONFIG_TOUCHSCREEN_STM_SPI) += $(TARGET).o
  endif
else
  ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
    $(TARGET)-objs := stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_mmap.o
    obj-$(CONFIG_TOUCHSCREEN_STM_SPI) += $(TARGET).o
  else
    $(TARGET)-objs := stm_core.o stm_fn.o stm_fw.o stm_dump.o stm_cmd.o stm_mmap.o stm_vendor.o
    obj-$(CONFIG_TOUCHSCREEN_STM_SPI) += $(TARGET).o
  endif
endif

ccflags-y += -Wformat

