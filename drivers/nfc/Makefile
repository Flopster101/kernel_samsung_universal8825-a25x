# SPDX-License-Identifier: GPL-2.0
#
# Makefile for nfc devices
#
ifneq ($(SEC_BUILD_CONF_USE_ESE_TZ), false)
	ccflags-y += -DENABLE_ESE_SPI_SECURED
endif

ifeq ($(CONFIG_SAMSUNG_NFC),m)
obj-$(CONFIG_SAMSUNG_NFC)		+= nfc_stm_sec.o
nfc_stm_sec-$(CONFIG_NFC_ST21NFC)		+= st21nfc.o
nfc_stm_sec-$(CONFIG_NFC_ST54SPI_ESE_SUPPORT)	+= st54spi.o
nfc_stm_sec-$(CONFIG_SEC_NFC_LOGGER)		+= nfc_logger/nfc_logger.o
else
obj-$(CONFIG_NFC_ST21NFC)		+= st21nfc.o
obj-$(CONFIG_NFC_ST54SPI_ESE_SUPPORT)	+= st54spi.o
obj-$(CONFIG_SEC_NFC_LOGGER)		+= nfc_logger/nfc_logger.o
endif

ccflags-y 			+= -DRECOVERY_ENABLE -UDEBUG


