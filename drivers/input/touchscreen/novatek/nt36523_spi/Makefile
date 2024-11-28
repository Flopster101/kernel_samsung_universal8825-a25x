#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

TARGET = novatek_ts_nt36523

$(TARGET)-objs	:= nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_sec_fn.o
ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui

obj-$(CONFIG_TOUCHSCREEN_NOVATEK_NT36523_SPI) += $(TARGET).o

ccflags-y += -Wformat
