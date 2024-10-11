# SPDX-License-Identifier: GPL-2.0
# UFSHCD makefile
ifdef CONFIG_SCSI_UFS_EXYNOS
ccflags-y += -I$(srctree)/drivers/scsi/ufs/$(TARGET_SOC)
endif

# The link order is important here. ufshcd-core must initialize
# before vendor drivers.
obj-$(CONFIG_SCSI_UFSHCD)		+= ufshcd-core.o
ufshcd-core-y				+= ufshcd.o ufs-sysfs.o
ufshcd-core-$(CONFIG_DEBUG_FS)		+= ufs-debugfs.o
ufshcd-core-$(CONFIG_SCSI_UFS_BSG)	+= ufs_bsg.o
ufshcd-core-$(CONFIG_SCSI_UFS_CRYPTO)	+= ufshcd-crypto.o
ufshcd-core-$(CONFIG_SCSI_UFS_HPB)	+= ufshpb.o

obj-$(CONFIG_SCSI_UFS_DWC_TC_PCI) += tc-dwc-g210-pci.o ufshcd-dwc.o tc-dwc-g210.o
obj-$(CONFIG_SCSI_UFS_DWC_TC_PLATFORM) += tc-dwc-g210-pltfrm.o ufshcd-dwc.o tc-dwc-g210.o
obj-$(CONFIG_SCSI_UFS_CDNS_PLATFORM) += cdns-pltfrm.o
obj-$(CONFIG_SCSI_UFS_QCOM) += ufs-qcom.o
obj-$(CONFIG_SCSI_UFSHCD) += ufshcd-core.o
ufshcd-core-y				+= ufshcd.o ufs-sysfs.o
ufshcd-core-$(CONFIG_DEBUG_FS)		+= ufs-debugfs.o
ufshcd-core-$(CONFIG_SCSI_UFS_BSG)	+= ufs_bsg.o
obj-$(CONFIG_SCSI_UFSHCD_PCI) += ufshcd-pci.o
obj-$(CONFIG_SCSI_UFSHCD_PLATFORM) += ufshcd-pltfrm.o
obj-$(CONFIG_SCSI_UFS_HISI) += ufs-hisi.o
obj-$(CONFIG_SCSI_UFS_MEDIATEK) += ufs-mediatek.o
ufshcd-core-$(CONFIG_SCSI_UFS_CRYPTO) += ufshcd-crypto.o
obj-$(CONFIG_SCSI_UFS_EXYNOS) += ufs-exynos-core.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_EXYNOS) += ufs-exynos.o ufs-exynos-dbg.o ufs-exynos-perf.o ufs-exynos-perf-v1.o ufs-exynos-gear.o\
					     $(TARGET_SOC)/ufs-cal-if.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_EXYNOS_FMP) += ufs-exynos-fmp.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_EXYNOS_SRPMB) += ufs-exynos-srpmb.o
ufs-exynos-core-$(CONFIG_SEC_UFS_FEATURE) += ufs-sec-feature.o ufs-sec-sysfs.o
