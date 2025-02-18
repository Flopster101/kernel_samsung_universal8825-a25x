fimc-is-objs	:= is-core.o \
		is-spi.o \
		is-framemgr.o \
		is-groupmgr.o \
		is-votfmgr.o \
		is-resourcemgr.o \
		is-devicemgr.o \
		is-video.o \
		is-video-sensor.o \
		is-video-sensor-subdev.o \
		is-video-3aa.o \
		is-video-mcs.o \
		is-video-mcs-subdev.o \
		is-video-pafrdma.o \
		is-video-ypp.o \
		is-subdev-ctrl.o \
		is-device-ischain.o \
		is-interface-wrap.o \
		is-device-sensor_v2.o \
		is-time.o \
		is-dt.o \
		is-i2c-config.o \
		is-device-camif-dma.o \
		pablo-device-iommu-group.o \
		pablo-device-camif-subblks.o

fimc-is-$(CONFIG_EXYNOS_DEVICE_MIPI_CSIS_VER4) += is-device-csi_v4.o

# video
fimc-is-$(CONFIG_PABLO_V8_1_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o \
		is-video-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o \
		is-video-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o \
		is-video-vra.o
fimc-is-$(CONFIG_PABLO_V9_1_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o
fimc-is-$(CONFIG_PABLO_V9_10_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-lme.o \
		is-video-lme-subdev.o
fimc-is-$(CONFIG_PABLO_V10_0_0) += is-video-byrp.o \
		is-video-rgbp.o \
		is-video-mcfp.o \
		is-video-yuvp.o \
		is-video-lme.o \
		is-video-lme-subdev.o
fimc-is-$(CONFIG_PABLO_V10_1_0) += is-video-byrp.o \
		is-video-rgbp.o \
		is-video-mcfp.o \
		is-video-yuvp.o \
		is-video-lme.o
fimc-is-$(CONFIG_PABLO_V10_20_0) += is-video-isp.o \
		is-video-3aa-subdev.o \
		is-video-isp-subdev.o \
		is-video-orb.o

# testing
fimc-is-objs += testing/pablo-test-sensor-self.o

# lib
fimc-is-objs += lib/is-hw-api-common.o \
		lib/is-binary.o \
		lib/is-debug.o \
		lib/is-work.o \
		lib/is-fpsimdmacro.o \
		lib/is-fpsimd.o \
		lib/is-uh_call.o \
		lib/is-irq.o \
		lib/pablo-mem.o
ifeq ($(CONFIG_CAMERA_DVFS_V2),y)
fimc-is-objs += is-dvfs_v2.o
else
fimc-is-objs += is-dvfs.o
endif
ifeq ($(CONFIG_PABLO_SOCKET_LAYER),y)
fimc-is-objs += lib/pablo-sock.o
endif
fimc-is-$(CONFIG_PABLO_OBTE_SUPPORT) += lib/pablo-obte.o

obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += fimc-is.o
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += sensor/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += ischain/

pablo-smc-objs := lib/pablo-smc.o
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += pablo-smc.o

ifeq ($(CONFIG_PABLO_KUNIT_TEST),m)
pablo-kunit-test-objs := testing/pablo-kunit-test.o
pablo-kunit-test-objs += testing/pablo-subdev-ctrl-kunit-test.o
pablo-kunit-test-objs += testing/pablo-core-kunit-test.o
pablo-kunit-test-objs += testing/pablo-device-csi-v4-kunit-test.o
pablo-kunit-test-objs += testing/pablo-device-sensor-v2-kunit-test.o
pablo-kunit-test-objs += testing/pablo-video-kunit-test.o
pablo-kunit-test-objs += testing/pablo-interface-ischain-kunit-test.o
pablo-kunit-test-objs += testing/pablo-time-kunit-test.o
pablo-kunit-test-objs += testing/pablo-interface-library-kunit-test.o
pablo-kunit-test-objs += testing/pablo-interface-sensor-kunit-test.o
pablo-kunit-test-objs += testing/pablo-hw-api-mcscaler-v10_0-kunit-test.o
pablo-kunit-test-objs += testing/pablo-hw-mcscaler-v3-kunit-test.o
pablo-kunit-test-objs += testing/pablo-hw-api-rgbp-v1_0-kunit-test.o
pablo-kunit-test-objs += testing/pablo-hw-api-pdp-v4_0-kunit-test.o
pablo-kunit-test-objs += testing/pablo-hw-api-byrp-v1_0-kunit-test.o
pablo-kunit-test-objs += testing/pablo-hw-api-yuvp-v1_0-kunit-test.o
pablo-kunit-test-objs += testing/pablo-hw-api-cstat-v1_0-kunit-test.o
pablo-kunit-test-$(CONFIG_DRCP_V1_0) += testing/pablo-hw-api-drcp-v1_0-kunit-test.o

pablo-kunit-test-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += testing/pablo-hw-api-common-dma-kunit-test.o

pablo-kunit-test-$(CONFIG_PABLO_OBTE_SUPPORT) += testing/pablo-lib-pablo-obte-kunit-test.o

pablo-kunit-test-$(CONFIG_CSIS_V8_0) += testing/pablo-hw-csi-v8_0-kunit-test.o

obj-$(CONFIG_PABLO_KUNIT_TEST) += pablo-kunit-test.o
endif

obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += vendor/

# hardware
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/is-hw-control.o \
		hardware/is-hw-dm.o

fimc-is-$(CONFIG_PABLO_V8_1_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-vra.o \
		hardware/is-hw-clahe.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-vra.o \
		hardware/is-hw-clahe.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-vra-v2.o \
		hardware/is-hw-clahe.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V9_1_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-lme.o \
		hardware/is-hw-yuvpp.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V9_10_0) += hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-lme.o \
		hardware/is-hw-yuvpp.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V10_0_0) += hardware/is-hw-cstat.o \
		hardware/is-hw-byrp.o \
		hardware/is-hw-rgbp.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-lme-v2.o \
		hardware/is-hw-yuvp.o \
		hardware/is-hw-mcfp.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V10_1_0) += hardware/is-hw-cstat.o \
		hardware/is-hw-byrp.o \
		hardware/is-hw-rgbp.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-lme-v2.o \
		hardware/is-hw-yuvp-v2.o \
		hardware/is-hw-mcfp.o \
		hardware/is-hw-cac-dummy.o
fimc-is-$(CONFIG_PABLO_V10_20_0) += hardware/is-hw-3aa-v2.o \
		hardware/is-hw-isp-v2.o \
		hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-orbmch.o \
		hardware/is-hw-cac.o

fimc-is-$(CONFIG_CAMERA_PDP) += hardware/is-hw-pdp.o

# hardware/api
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/api/is-hw-common-dma.o

fimc-is-$(CONFIG_MC_SCALER_V8_1) += hardware/api/is-hw-api-mcscaler-v8_1.o
fimc-is-$(CONFIG_MC_SCALER_V8_10) += hardware/api/is-hw-api-mcscaler-v8_10.o
fimc-is-$(CONFIG_MC_SCALER_V8_20) += hardware/api/is-hw-api-mcscaler-v8_20-v8_21.o
fimc-is-$(CONFIG_MC_SCALER_V9_1) += hardware/api/is-hw-api-mcscaler-v9_1.o
fimc-is-$(CONFIG_MC_SCALER_V9_10) += hardware/api/is-hw-api-mcscaler-v9_10.o
fimc-is-$(CONFIG_MC_SCALER_V10_0) += hardware/api/is-hw-api-mcscaler-v10_0.o
fimc-is-$(CONFIG_MC_SCALER_V10_1) += hardware/api/is-hw-api-mcscaler-v10_0.o
fimc-is-$(CONFIG_MC_SCALER_V10_20) += hardware/api/is-hw-api-mcscaler-v10_20.o

fimc-is-$(CONFIG_PABLO_V8_1_0) += hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += hardware/api/is-hw-api-vra-v3_2.o

fimc-is-$(CONFIG_PDP_V1_0_0) += hardware/api/is-hw-api-pdp-v1_0_0.o
fimc-is-$(CONFIG_PDP_V3_1) += hardware/api/is-hw-api-pdp-v3_1.o
fimc-is-$(CONFIG_PDP_V4_0) += hardware/api/is-hw-api-pdp-v4_0.o
fimc-is-$(CONFIG_PDP_V4_1) += hardware/api/is-hw-api-pdp-v4_0.o
fimc-is-$(CONFIG_CSTAT_V1_0) += hardware/api/is-hw-api-cstat-v1_0.o
fimc-is-$(CONFIG_CSTAT_V1_1) += hardware/api/is-hw-api-cstat-v1_1.o
fimc-is-$(CONFIG_BYRP_V1_0) += hardware/api/is-hw-api-byrp-v1_0.o
fimc-is-$(CONFIG_BYRP_V1_1) += hardware/api/is-hw-api-byrp-v1_1.o
fimc-is-$(CONFIG_RGBP_V1_0) += hardware/api/is-hw-api-rgbp-v1_0.o
fimc-is-$(CONFIG_RGBP_V1_1) += hardware/api/is-hw-api-rgbp-v1_1.o
fimc-is-$(CONFIG_YUVPP_V2_1) += hardware/api/is-hw-api-yuvpp-v2_1.o
fimc-is-$(CONFIG_LME_V1_1) += hardware/api/is-hw-api-lme-v1_1.o
fimc-is-$(CONFIG_LME_V2_0) += hardware/api/is-hw-api-lme-v2_0.o
fimc-is-$(CONFIG_LME_V2_1) += hardware/api/is-hw-api-lme-v2_1.o
fimc-is-$(CONFIG_MCFP_V10_0) += hardware/api/is-hw-api-mcfp-v10_0.o
fimc-is-$(CONFIG_MCFP_V10_1) += hardware/api/is-hw-api-mcfp-v10_1.o
fimc-is-$(CONFIG_YUVP_V1_0) += hardware/api/is-hw-api-yuvp-v1_0.o
fimc-is-$(CONFIG_YUVP_V1_1) += hardware/api/is-hw-api-yuvp-v1_1.o
fimc-is-$(CONFIG_DRCP_V1_0) += hardware/api/is-hw-api-drcp-v1_0.o
fimc-is-$(CONFIG_TAA_V9_20) += hardware/api/is-hw-api-3aa-v9_20.o
fimc-is-$(CONFIG_DNS_V1_20) += hardware/api/is-hw-api-dns-v1_20.o
fimc-is-$(CONFIG_ITP_V1_20) += hardware/api/is-hw-api-itp-v1_20.o
fimc-is-$(CONFIG_ORBMCH_V2_1) += hardware/api/is-hw-api-orbmch-v2_1.o
fimc-is-$(CONFIG_MCFP_V10_20) += hardware/api/is-hw-api-mcfp-v10_20.o

# interface
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += interface/is-interface-ischain.o \
		interface/is-interface-library.o \
		interface/is-interface-ddk.o

fimc-is-$(CONFIG_PABLO_V8_1_0) += interface/is-interface-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += interface/is-interface-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += interface/is-interface-vra-v2.o

# ischain
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += ischain/setup-is-module.o

fimc-is-$(CONFIG_PABLO_V9_1_0) += ischain/is-v9_1_0/setup-is.o \
		ischain/is-v9_1_0/setup-is-sensor.o \
		ischain/is-v9_1_0/is-hw-pwr.o \
		ischain/is-v9_1_0/is-hw-chain.o \
		ischain/is-v9_1_0/is-hw-dvfs.o \
		ischain/is-v9_1_0/is-subdev-3aa.o \
		ischain/is-v9_1_0/is-subdev-3ac.o \
		ischain/is-v9_1_0/is-subdev-3ap.o \
		ischain/is-v9_1_0/is-subdev-3af.o \
		ischain/is-v9_1_0/is-subdev-3ag.o \
		ischain/is-v9_1_0/is-subdev-3ao.o \
		ischain/is-v9_1_0/is-subdev-3al.o \
		ischain/is-v9_1_0/is-subdev-isp.o \
		ischain/is-v9_1_0/is-subdev-ixc.o \
		ischain/is-v9_1_0/is-subdev-ixp.o \
		ischain/is-v9_1_0/is-subdev-ixg.o \
		ischain/is-v9_1_0/is-subdev-ixt.o \
		ischain/is-v9_1_0/is-subdev-ixv.o \
		ischain/is-v9_1_0/is-subdev-ixw.o \
		ischain/is-v9_1_0/is-subdev-mexc.o \
		ischain/is-v9_1_0/is-subdev-orbxc.o \
		ischain/is-v9_1_0/is-subdev-mcs.o \
		ischain/is-v9_1_0/is-subdev-mcsp.o \
		ischain/is-v9_1_0/is-subdev-paf.o \
		ischain/is-v9_1_0/is-subdev-ypp.o \
		ischain/is-v9_1_0/is-subdev-lme.o \
		ischain/is-v9_1_0/is-subdev-lmes.o \
		ischain/is-v9_1_0/is-subdev-lmec.o \
		ischain/is-v9_1_0/is-subdev-sensor.o \
		ischain/is-v9_1_0/is-subdev-sensor-vc.o \
		ischain/is-v9_1_0/is-votf.o \
		ischain/is-v9_1_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V9_10_0) += ischain/is-v9_10_0/setup-is.o \
		ischain/is-v9_10_0/setup-is-sensor.o \
		ischain/is-v9_10_0/is-hw-pwr.o \
		ischain/is-v9_10_0/is-hw-chain.o \
		ischain/is-v9_10_0/is-hw-dvfs.o \
		ischain/is-v9_10_0/is-subdev-3aa.o \
		ischain/is-v9_10_0/is-subdev-3ac.o \
		ischain/is-v9_10_0/is-subdev-3ap.o \
		ischain/is-v9_10_0/is-subdev-3af.o \
		ischain/is-v9_10_0/is-subdev-3ag.o \
		ischain/is-v9_10_0/is-subdev-3ao.o \
		ischain/is-v9_10_0/is-subdev-3al.o \
		ischain/is-v9_10_0/is-subdev-isp.o \
		ischain/is-v9_10_0/is-subdev-ixc.o \
		ischain/is-v9_10_0/is-subdev-ixp.o \
		ischain/is-v9_10_0/is-subdev-ixg.o \
		ischain/is-v9_10_0/is-subdev-ixt.o \
		ischain/is-v9_10_0/is-subdev-ixv.o \
		ischain/is-v9_10_0/is-subdev-ixw.o \
		ischain/is-v9_10_0/is-subdev-mexc.o \
		ischain/is-v9_10_0/is-subdev-orbxc.o \
		ischain/is-v9_10_0/is-subdev-mcs.o \
		ischain/is-v9_10_0/is-subdev-mcsp.o \
		ischain/is-v9_10_0/is-subdev-paf.o \
		ischain/is-v9_10_0/is-subdev-ypp.o \
		ischain/is-v9_10_0/is-subdev-lme.o \
		ischain/is-v9_10_0/is-subdev-lmes.o \
		ischain/is-v9_10_0/is-subdev-lmec.o \
		ischain/is-v9_10_0/is-subdev-sensor.o \
		ischain/is-v9_10_0/is-subdev-sensor-vc.o \
		ischain/is-v9_10_0/is-votf.o \
		ischain/is-v9_10_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V10_0_0) += ischain/is-v10_0_0/setup-is.o \
		ischain/is-v10_0_0/setup-is-sensor.o \
		ischain/is-v10_0_0/is-hw-pwr.o \
		ischain/is-v10_0_0/is-hw-chain.o \
		ischain/is-v10_0_0/is-hw-dvfs.o \
		ischain/is-v10_0_0/is-subdev-cstat.o \
		ischain/is-v10_0_0/is-subdev-byrp.o \
		ischain/is-v10_0_0/is-subdev-rgbp.o \
		ischain/is-v10_0_0/is-subdev-mcs.o \
		ischain/is-v10_0_0/is-subdev-mcsp.o \
		ischain/is-v10_0_0/is-subdev-paf.o \
		ischain/is-v10_0_0/is-subdev-yuvp.o \
		ischain/is-v10_0_0/is-subdev-mcfp.o \
		ischain/is-v10_0_0/is-subdev-lme.o \
		ischain/is-v10_0_0/is-subdev-sensor.o \
		ischain/is-v10_0_0/is-subdev-sensor-vc.o \
		ischain/is-v10_0_0/is-votf.o \
		ischain/is-v10_0_0/is-stripe.o \
		ischain/is-v10_0_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V10_1_0) += ischain/is-v10_1_0/setup-is.o \
		ischain/is-v10_1_0/setup-is-sensor.o \
		ischain/is-v10_1_0/is-hw-pwr.o \
		ischain/is-v10_1_0/is-hw-chain.o \
		ischain/is-v10_1_0/is-hw-dvfs.o \
		ischain/is-v10_1_0/is-subdev-cstat.o \
		ischain/is-v10_1_0/is-subdev-byrp.o \
		ischain/is-v10_1_0/is-subdev-rgbp.o \
		ischain/is-v10_1_0/is-subdev-mcs.o \
		ischain/is-v10_1_0/is-subdev-mcsp.o \
		ischain/is-v10_1_0/is-subdev-paf.o \
		ischain/is-v10_1_0/is-subdev-yuvp.o \
		ischain/is-v10_1_0/is-subdev-mcfp.o \
		ischain/is-v10_1_0/is-subdev-lme.o \
		ischain/is-v10_1_0/is-subdev-sensor.o \
		ischain/is-v10_1_0/is-subdev-sensor_vc.o \
		ischain/is-v10_1_0/is-votf.o \
		ischain/is-v10_1_0/is-stripe.o \
		ischain/is-v10_1_0/is-subdev-framemgr.o

fimc-is-$(CONFIG_PABLO_V10_20_0) += ischain/is-v10_20_0/setup-is.o \
		ischain/is-v10_20_0/setup-is-sensor.o \
		ischain/is-v10_20_0/is-hw-pwr.o \
		ischain/is-v10_20_0/is-hw-chain.o \
		ischain/is-v10_20_0/is-hw-dvfs.o \
		ischain/is-v10_20_0/is-subdev-3aa.o \
		ischain/is-v10_20_0/is-subdev-3ac.o \
		ischain/is-v10_20_0/is-subdev-3ap.o \
		ischain/is-v10_20_0/is-subdev-3af.o \
		ischain/is-v10_20_0/is-subdev-3ag.o \
		ischain/is-v10_20_0/is-subdev-3ao.o \
		ischain/is-v10_20_0/is-subdev-3al.o \
		ischain/is-v10_20_0/is-subdev-isp.o \
		ischain/is-v10_20_0/is-subdev-ixc.o \
		ischain/is-v10_20_0/is-subdev-ixp.o \
		ischain/is-v10_20_0/is-subdev-ixg.o \
		ischain/is-v10_20_0/is-subdev-ixt.o \
		ischain/is-v10_20_0/is-subdev-ixv.o \
		ischain/is-v10_20_0/is-subdev-ixw.o \
		ischain/is-v10_20_0/is-subdev-mexc.o \
		ischain/is-v10_20_0/is-subdev-orbxc.o \
		ischain/is-v10_20_0/is-subdev-orb.o \
		ischain/is-v10_20_0/is-subdev-mcs.o \
		ischain/is-v10_20_0/is-subdev-mcsp.o \
		ischain/is-v10_20_0/is-subdev-paf.o \
		ischain/is-v10_20_0/is-subdev-sensor.o \
		ischain/is-v10_20_0/is-subdev-sensor-vc.o \
		ischain/is-v10_20_0/is-votf.o \
		ischain/is-v10_20_0/is-subdev-framemgr.o

# sensor/csi
fimc-is-$(CONFIG_CSIS_V1_2) += csi/is-hw-csi-v1_2.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V5_3) += csi/is-hw-csi-v5_3.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V5_4) += csi/is-hw-csi-v5_4.o \
				csi/is-hw-phy.o
fimc-is-$(CONFIG_CSIS_V8_0) += csi/is-hw-csi-v8_0.o \
				csi/is-hw-phy.o

# sensor/module_framework
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP)	+= sensor/is-helper-i2c.o \
						   sensor/module_framework/is-interface-sensor.o \
						   sensor/module_framework/is-interface-actuator.o \
						   sensor/module_framework/is-control-sensor.o \
						   sensor/module_framework/is-control-actuator.o \
						   sensor/module_framework/is-device-sensor-peri.o \
						   sensor/module_framework/modules/is-device-module-base.o \
						   sensor/module_framework/cis/is-cis.o

# for zebu
fimc-is-$(CONFIG_CAMERA_CIS_ZEBU_OBJ)		+= sensor/module_framework/modules/is-device-module-zebu.o

# vendor
fimc-is-$(CONFIG_CAMERA_VENDER_DEFAULT) += vendor/default/is-vender.o \
				    vendor/default/is-sysfs.o

subdir-ccflags-y := -I$(srctree)/drivers/media/platform/exynos/camera
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/interface
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware/api
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/vendor
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/sensor/module_framework
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_20_0
subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v9_1_0
subdir-ccflags-$(CONFIG_PABLO_V9_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v9_10_0
subdir-ccflags-$(CONFIG_PABLO_V10_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v10_0_0
subdir-ccflags-$(CONFIG_PABLO_V10_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v10_1_0
subdir-ccflags-$(CONFIG_PABLO_V10_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v10_20_0
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v8_20_0
subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v9_1_0
subdir-ccflags-$(CONFIG_PABLO_V9_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v9_10_0
subdir-ccflags-$(CONFIG_PABLO_V10_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v10_0_0
subdir-ccflags-$(CONFIG_PABLO_V10_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v10_1_0
subdir-ccflags-$(CONFIG_PABLO_V10_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/include/v10_20_0
# For using vendor specific code, please rename default to xxx.
subdir-ccflags-$(CONFIG_CAMERA_VENDER_DEFAULT) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default
ifeq ($(CONFIG_CAMERA_VENDER_DEFAULT),y)
#subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_1_0
#subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_10_0
#subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_20_0
#subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v9_1_0
#subdir-ccflags-$(CONFIG_PABLO_V9_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v9_10_0
#subdir-ccflags-$(CONFIG_PABLO_V10_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v10_0_0
#subdir-ccflags-$(CONFIG_PABLO_V10_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v10_1_0
#subdir-ccflags-$(CONFIG_PABLO_V10_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v10_20_0
endif

ifeq ($(CONFIG_CAMERA_VENDER_MCD),y)
fimc-is-$(CONFIG_CAMERA_VENDER_MCD) += vendor/mcd/is-vender.o \
				vendor/mcd/is-vender-caminfo.o \
				vendor/mcd/is-sec-define.o \
				vendor/mcd/is-notifier.o \
				vendor/mcd/crc32.o \
				vendor/mcd/is-sysfs.o

fimc-is-$(CONFIG_OIS_USE)			+= vendor/mcd/is-device-ois_common.o
fimc-is-$(CONFIG_AF_HOST_CONTROL)		+= vendor/mcd/is-device-af.o
fimc-is-$(CONFIG_CAMERA_USE_INTERNAL_MCU) 	+= vendor/mcd/is-ois-mcu.o \
						vendor/mcd/is-hw-api-ois-mcu-v1_1_1.o

else ifeq ($(CONFIG_CAMERA_VENDER_MCD_V2),y)
fimc-is-$(CONFIG_CAMERA_VENDER_MCD_V2) += vendor/mcd_v2/is-vender.o \
				vendor/mcd_v2/is-vender-caminfo.o \
				vendor/mcd_v2/is-sec-define.o \
				vendor/mcd_v2/crc32.o \
				vendor/mcd_v2/is-sysfs.o

fimc-is-$(CONFIG_OIS_USE)			+= vendor/mcd_v2/is-device-ois_common.o
fimc-is-$(CONFIG_AF_HOST_CONTROL)		+= vendor/mcd_v2/is-device-af.o
fimc-is-$(CONFIG_CAMERA_USE_INTERNAL_MCU) 	+= vendor/mcd_v2/is-ois-mcu.o \
						vendor/mcd_v2/is-hw-api-ois-mcu-v1_1_1.o
fimc-is-$(CONFIG_CAMERA_USE_MCU)		+= vendor/mcd_v2/is-device-ois-mcu.o
fimc-is-$(CONFIG_CAMERA_USE_AOIS)		+= vendor/mcd_v2/is-aois-if.o
endif

ifeq ($(CONFIG_CAMERA_VENDER_MCD),y)
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd
#subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v8_1_0
#subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v8_10_0
#subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v8_20_0
#subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v9_1_0
#subdir-ccflags-$(CONFIG_PABLO_V9_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v9_10_0
#subdir-ccflags-$(CONFIG_PABLO_V10_0_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v10_0_0
else ifeq ($(CONFIG_CAMERA_VENDER_MCD_V2),y)
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd_v2
endif

subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera-pp
