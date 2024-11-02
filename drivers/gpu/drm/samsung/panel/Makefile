#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

subdir-ccflags-y := -I$(srctree) -I$(srctree)/$(src)

mcd-panel-y += timenval.o \
				abd.o \
				util.o \
				panel_lib.o \
				maptbl.o \
				panel_sequence.o \
				panel_expression.o \
				panel_condition.o \
				panel_property.o \
				panel_delay.o \
				panel_packet.o \
				panel_resource.o \
				panel_dump.o \
				panel_config.o \
				panel.o \
				panel_bl.o \
				dimming.o \
				panel_gpio.o \
				panel_regulator.o \
				panel_drv_ioctl.o \
				panel_drv.o \
				panel_irc.o \
				panel_vrr.o \
				panel_power_ctrl.o \
				panel_obj.o \
				panel_function.o \
				panel_firmware.o \
				panel_mutex.o

mcd-panel-$(CONFIG_USDM_PANEL_DISPLAY_MODE) += panel_modes.o
mcd-panel-$(CONFIG_USDM_PANEL_SYSFS) += sysfs.o
mcd-panel-$(CONFIG_USDM_MDNIE) += mdnie.o
mcd-panel-$(CONFIG_USDM_PANEL_DEBUG) += panel_debug.o
mcd-panel-$(CONFIG_USDM_PANEL_SELF_DISPLAY) += aod/aod_drv.o
mcd-panel-$(CONFIG_USDM_PANEL_FREQ_HOP) += panel_freq_hop.o
mcd-panel-$(CONFIG_USDM_PANEL_MAFPC) += mafpc/mafpc_drv.o
mcd-panel-$(CONFIG_USDM_PANEL_DPUI) += dpui.o
mcd-panel-$(CONFIG_USDM_PANEL_COPR) += copr.o
mcd-panel-$(CONFIG_USDM_POC_SPI) += panel_poc.o panel_spi.o
mcd-panel-$(CONFIG_USDM_BLIC_I2C) += panel_i2c.o
mcd-panel-$(CONFIG_USDM_PANEL_BLIC) += panel_blic.o
mcd-panel-$(CONFIG_USDM_LPD_AUTO_BR) += panel_lpd_cmd.o
mcd-panel-$(CONFIG_USDM_ADAPTIVE_MIPI) += adaptive_mipi.o
mcd-panel-$(CONFIG_USDM_SDP_ADAPTIVE_MIPI) += sdp_adaptive_mipi.o
mcd-panel-$(CONFIG_USDM_PANEL_TESTMODE) += panel_testmode.o

mcd-panel-$(CONFIG_USDM_PANEL_JSON) += \
	ezop/jsmn.o \
	ezop/json.o \
	ezop/json_writer.o \
	ezop/json_reader.o \
	ezop/panel_json.o

obj-$(CONFIG_USDM_PANEL) += mcd-panel.o

subdir-ccflags-y += \
    -Wno-implicit-fallthrough

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
    -Wformat-security \
    -Wunused-const-variable \
    -Wframe-larger-than=8192 \
    -Wunused-variable \
    -Wunused-function \
    -Wno-missing-braces \
    -Wformat

