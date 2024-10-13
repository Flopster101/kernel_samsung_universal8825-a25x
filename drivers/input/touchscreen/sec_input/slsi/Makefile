TARGET = slsi_ts

ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
  $(TARGET)-objs := slsi_core.o slsi_fn.o slsi_fw.o slsi_cmd.o slsi_dump.o slsi_ioctrl.o
  obj-$(CONFIG_TOUCHSCREEN_SLSI) += $(TARGET).o
else
  $(TARGET)-objs := slsi_core.o slsi_fn.o slsi_fw.o slsi_cmd.o slsi_dump.o slsi_ioctrl.o slsi_vendor.o
  obj-$(CONFIG_TOUCHSCREEN_SLSI) += $(TARGET).o
endif

ccflags-y += -Wformat

