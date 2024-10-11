# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_PHY_EXYNOS_DP_VIDEO)	+= phy-exynos-dp-video.o
obj-$(CONFIG_PHY_EXYNOS_MIPI_VIDEO)	+= phy-exynos-mipi-video.o
obj-$(CONFIG_PHY_EXYNOS_MIPI_DSIM)	+= phy-exynos-mipi-dsim.o
obj-$(CONFIG_PHY_EXYNOS_MIPI)       += phy-exynos-mipi.o
obj-$(CONFIG_PHY_EXYNOS_PCIE)		+= phy-exynos-pcie.o
obj-$(CONFIG_PHY_SAMSUNG_USB2)		+= phy-exynos-usb2.o
phy-exynos-usb2-y			+= phy-samsung-usb2.o
phy-exynos-usb2-$(CONFIG_PHY_EXYNOS4210_USB2)	+= phy-exynos4210-usb2.o
phy-exynos-usb2-$(CONFIG_PHY_EXYNOS4X12_USB2)	+= phy-exynos4x12-usb2.o
phy-exynos-usb2-$(CONFIG_PHY_EXYNOS5250_USB2)	+= phy-exynos5250-usb2.o
phy-exynos-usb2-$(CONFIG_PHY_S5PV210_USB2)	+= phy-s5pv210-usb2.o
obj-$(CONFIG_PHY_EXYNOS5_USBDRD)	+= phy-exynos5-usbdrd.o
ifneq ($(CONFIG_PHY_EXYNOS_USBDRD_EUSB),)
obj-$(CONFIG_PHY_EXYNOS_USBDRD_EUSB)	+= phy-exynos-usbdrd-super.o
phy-exynos-usbdrd-super-y		+= phy-exynos-usbdrd-eusb.o
phy-exynos-usbdrd-super-y		+= phy-exynos-eusb.o
phy-exynos-usbdrd-super-y		+= exynos-usb-blkcon.o
phy-exynos-usbdrd-super-y		+= phy-exynos-snps-usbdp.o
else
ifneq ($(CONFIG_PHY_EXYNOS_USBDRD),)
obj-$(CONFIG_PHY_EXYNOS_USBDRD)		+= phy-exynos-usbdrd-super.o
phy-exynos-usbdrd-super-y		+= phy-exynos-usbdrd.o
phy-exynos-usbdrd-super-y      		+= phy-exynos-usb3p1.o
endif
endif

obj-$(CONFIG_PHY_EXYNOS5250_SATA)	+= phy-exynos5250-sata.o
