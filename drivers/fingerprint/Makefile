#
# Makefile for the Fingerprint sensor driver.
#

# Each configuration option enables a list of files.

ccflags-y := -Wformat

obj-$(CONFIG_SENSORS_FINGERPRINT) += fingerprint.o fingerprint_sysfs.o
obj-$(CONFIG_SENSORS_ET5XX)	+= fp_et5xx.o
obj-$(CONFIG_SENSORS_EC6XX)	+= fp_ec6xx.o
obj-$(CONFIG_SENSORS_EL7XX) += fp_el7xx.o
obj-$(CONFIG_SENSORS_GW3X) += fp_gw3x.o
obj-$(CONFIG_SENSORS_GW9558X) += fp_gw9558x.o
obj-$(CONFIG_SENSORS_QBT2000) += fp_qbt2000.o
obj-$(CONFIG_SENSORS_QFS4008) += fp_qfs4008.o

fingerprint-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint_common.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_LSI)	+= fingerprint_common_lsi.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_QCOM)	+= fingerprint_common_qcom.o
fingerprint-$(CONFIG_SENSORS_FINGERPRINT_MTK)	+= fingerprint_common_mtk.o

fp_et5xx-$(CONFIG_SENSORS_ET5XX)		+= et5xx-spi.o et5xx-spi_data_transfer.o
fp_ec6xx-$(CONFIG_SENSORS_EC6XX)		+= ec6xx-spi.o ec6xx-spi_data_transfer.o
fp_el7xx-$(CONFIG_SENSORS_EL7XX)		+= el7xx-spi.o el7xx-spi_data_transfer.o
fp_gw3x-$(CONFIG_SENSORS_GW3X)		+= gw3x_common.o gw3x_spidev.o
fp_gw9558x-$(CONFIG_SENSORS_GW9558X)	+= gw9558x_common.o gw9558x_spidev.o
fp_qbt2000-$(CONFIG_SENSORS_QBT2000)	+= qbt2000_common.o
fp_qbt2000-$(CONFIG_SENSORS_QBT2000)			+= qbt2000_spidev.o
fp_qfs4008-$(CONFIG_SENSORS_QFS4008)	+= qfs4008_common.o
fp_qfs4008-$(CONFIG_SENSORS_QFS4008)			+= qfs4008_spidev.o