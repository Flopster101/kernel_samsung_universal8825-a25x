#include "../../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_vclk_vdd_mif[] = {
	PLL_MIF,
};
enum clk_id cmucal_vclk_vdd_cpucl0[] = {
	PLL_CPUCL0,
	DIV_CLK_CLUSTER0_ACLK,
	PLL_DSU,
};
enum clk_id cmucal_vclk_vdd_alive[] = {
	MUX_CLK_CMGP_USI1,
	MUX_CLK_CMGP_USI2,
	MUX_CLK_CMGP_USI3,
};
enum clk_id cmucal_vclk_vddi[] = {
	MUX_CLKCMU_MIF_SWITCH,
	CLKCMU_CPUCL0_SWITCH,
	MUX_CLKCMU_CPUCL0_SWITCH,
	CLKCMU_CPUCL1_SWITCH,
	MUX_CLKCMU_CPUCL1_SWITCH,
	CLKCMU_PERI_MMC_CARD,
	CLKCMU_G3D_SWITCH,
	MUX_CLKCMU_G3D_SWITCH,
	CLKCMU_AUD_CPU,
	CLKCMU_NPUS_BUS,
	MUX_CLKCMU_NPUS_BUS,
	CLKCMU_DSU_SWITCH,
	MUX_CLKCMU_DSU_SWITCH,
	CLKCMU_CORE_BUS,
	MUX_CLKCMU_CORE_BUS,
	CLKCMU_CORE_SSS,
	CLKCMU_BUSC_BUS,
	MUX_CLKCMU_BUSC_BUS,
	CLKCMU_HSI_UFS_EMBD,
	CLKCMU_M2M_MSCL,
	MUX_CLKCMU_M2M_MSCL,
	CLKCMU_MCSC_BUS,
	MUX_CLKCMU_MCSC_BUS,
	CLKCMU_MCSC_GDC,
	MUX_CLKCMU_MCSC_GDC,
	CLKCMU_MFC_MFC,
	MUX_CLKCMU_MFC_MFC,
	CLKCMU_NPU0_BUS,
	MUX_CLKCMU_NPU0_BUS,
	CLKCMU_TAA_BUS,
	MUX_CLKCMU_TAA_BUS,
	CLKCMU_ISP_BUS,
	MUX_CLKCMU_ISP_BUS,
	CLKCMU_CSIS_BUS,
	MUX_CLKCMU_CSIS_BUS,
	CLKCMU_CORE_G3D,
	CLKCMU_TNR_BUS,
	MUX_CLKCMU_TNR_BUS,
	CLKCMU_CPUCL0_DBG_BUS,
	MUX_CLKCMU_CPUCL0_DBG_BUS,
	CLKCMU_CPUCL0_BUSP,
	MUX_CLKCMU_CPUCL0_BUSP,
	CLKCMU_DPU_BUS,
	MUX_CLKCMU_DPU_BUS,
	CLKCMU_MCSC_MCSC,
	MUX_CLKCMU_MCSC_MCSC,
	CLKCMU_AUD_BUS,
	MUX_CLKCMU_AUD_BUS,
	CLKCMU_PERI_BUS,
	MUX_CLKCMU_PERI_BUS,
	CLKCMU_MIF_BUSP,
	CLKCMU_PERI_IP,
	CLKCMU_CMU_BOOST,
	CLKCMU_HSI_BUS,
	MUX_CLKCMU_HSI_BUS,
	PLL_SHARED0,
	PLL_G3D,
	PLL_MMC,
};
enum clk_id cmucal_vclk_vdd_cpucl1[] = {
	PLL_CPUCL1,
	MUX_CPUCL1_CMUREF,
};
enum clk_id cmucal_vclk_vdd_aud[] = {
	DIV_CLK_AUD_CPU,
	MUX_CLK_AUD_CPU,
	MUX_CLK_AUD_CPU_PLL,
	CLK_AUD_DMIC,
	DIV_CLK_AUD_CNT,
	CLKAUD_USB_BUS,
	CLKAUD_USB_USB20DRD,
	MUX_CLK_AUD_PCMC,
	DIV_CLK_AUD_AUDIF,
	PLL_AUD,
};
/* SPECIAL VCLK -> Clock Node List */
enum clk_id cmucal_vclk_mux_clk_alive_i3c_pmic[] = {
	MUX_CLK_ALIVE_I3C_PMIC,
	DIV_CLK_ALIVE_I3C_PMIC,
};
enum clk_id cmucal_vclk_clkcmu_alive_bus[] = {
	CLKCMU_ALIVE_BUS,
	MUX_CLKCMU_ALIVE_BUS,
};
enum clk_id cmucal_vclk_div_clk_aud_pcmc[] = {
	DIV_CLK_AUD_PCMC,
	DIV_CLK_AUD_CPU_PCLKDBG,
	DIV_CLK_AUD_UAIF0,
	MUX_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF1,
	MUX_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF2,
	MUX_CLK_AUD_UAIF2,
	DIV_CLK_AUD_UAIF3,
	MUX_CLK_AUD_UAIF3,
	DIV_CLK_AUD_UAIF4,
	MUX_CLK_AUD_UAIF4,
	DIV_CLK_AUD_DSIF,
	MUX_CLK_AUD_DSIF,
	DIV_CLK_AUD_FM,
	MUX_CLK_AUD_FM,
	DIV_CLK_AUD_FM_SPDY,
	DIV_CLK_AUD_UAIF5,
	MUX_CLK_AUD_UAIF5,
	DIV_CLK_AUD_UAIF6,
	MUX_CLK_AUD_UAIF6,
	DIV_CLK_AUD_CPU_ACP,
};
enum clk_id cmucal_vclk_mux_busc_cmuref[] = {
	MUX_BUSC_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost[] = {
	MUX_CLKCMU_CMU_BOOST,
};
enum clk_id cmucal_vclk_mux_clk_chub_timer[] = {
	MUX_CLK_CHUB_TIMER,
};
enum clk_id cmucal_vclk_mux_cmu_cmuref[] = {
	MUX_CMU_CMUREF,
};
enum clk_id cmucal_vclk_mux_core_cmuref[] = {
	MUX_CORE_CMUREF,
};
enum clk_id cmucal_vclk_mux_cpucl0_cmuref[] = {
	MUX_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_mux_dsu_cmuref[] = {
	MUX_DSU_CMUREF,
};
enum clk_id cmucal_vclk_mux_mif_cmuref[] = {
	MUX_MIF_CMUREF,
};
enum clk_id cmucal_vclk_mux_clk_usb_usb20drd[] = {
	MUX_CLK_USB_USB20DRD,
};
enum clk_id cmucal_vclk_clkcmu_usb_usb20drd[] = {
	CLKCMU_USB_USB20DRD,
	MUX_CLKCMU_USB_USB20DRD,
};
enum clk_id cmucal_vclk_clkcmu_dpu_dsim[] = {
	CLKCMU_DPU_DSIM,
	MUX_CLKCMU_DPU_DSIM,
};
enum clk_id cmucal_vclk_mux_clkcmu_peri_mmc_card[] = {
	MUX_CLKCMU_PERI_MMC_CARD,
};
enum clk_id cmucal_vclk_div_clk_alive_dbgcore_uart[] = {
	DIV_CLK_ALIVE_DBGCORE_UART,
	MUX_CLK_ALIVE_DBGCORE_UART,
};
enum clk_id cmucal_vclk_div_clk_alive_usi0[] = {
	DIV_CLK_ALIVE_USI0,
	MUX_CLK_ALIVE_USI0,
};
enum clk_id cmucal_vclk_div_clk_alive_i2c[] = {
	DIV_CLK_ALIVE_I2C,
	MUX_CLK_ALIVE_I2C,
};
enum clk_id cmucal_vclk_mux_clkcmu_aud_cpu[] = {
	MUX_CLKCMU_AUD_CPU,
};
enum clk_id cmucal_vclk_div_clk_aud_mclk[] = {
	DIV_CLK_AUD_MCLK,
};
enum clk_id cmucal_vclk_div_clk_chub_usi0[] = {
	DIV_CLK_CHUB_USI0,
	MUX_CLK_CHUB_USI0,
};
enum clk_id cmucal_vclk_clkcmu_chub_peri[] = {
	CLKCMU_CHUB_PERI,
	MUX_CLKCMU_CHUB_PERI,
};
enum clk_id cmucal_vclk_div_clk_chub_usi1[] = {
	DIV_CLK_CHUB_USI1,
	MUX_CLK_CHUB_USI1,
};
enum clk_id cmucal_vclk_div_clk_chub_usi2[] = {
	DIV_CLK_CHUB_USI2,
	MUX_CLK_CHUB_USI2,
};
enum clk_id cmucal_vclk_div_clk_chub_usi3[] = {
	DIV_CLK_CHUB_USI3,
	MUX_CLK_CHUB_USI3,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi0[] = {
	DIV_CLK_CMGP_USI0,
	MUX_CLK_CMGP_USI0,
};
enum clk_id cmucal_vclk_clkcmu_cmgp_peri[] = {
	CLKCMU_CMGP_PERI,
	MUX_CLKCMU_CMGP_PERI,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi4[] = {
	DIV_CLK_CMGP_USI4,
	MUX_CLK_CMGP_USI4,
};
enum clk_id cmucal_vclk_div_clk_cmgp_i3c[] = {
	DIV_CLK_CMGP_I3C,
	MUX_CLK_CMGP_I3C,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi1[] = {
	DIV_CLK_CMGP_USI1,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi2[] = {
	DIV_CLK_CMGP_USI2,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi3[] = {
	DIV_CLK_CMGP_USI3,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk0[] = {
	CLKCMU_CIS_CLK0,
	MUX_CLKCMU_CIS_CLK0,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk1[] = {
	CLKCMU_CIS_CLK1,
	MUX_CLKCMU_CIS_CLK1,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk2[] = {
	CLKCMU_CIS_CLK2,
	MUX_CLKCMU_CIS_CLK2,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk3[] = {
	CLKCMU_CIS_CLK3,
	MUX_CLKCMU_CIS_CLK3,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk4[] = {
	CLKCMU_CIS_CLK4,
	MUX_CLKCMU_CIS_CLK4,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk5[] = {
	CLKCMU_CIS_CLK5,
	MUX_CLKCMU_CIS_CLK5,
};
enum clk_id cmucal_vclk_div_clk_cpucl0_shortstop[] = {
	DIV_CLK_CPUCL0_SHORTSTOP,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_shortstop[] = {
	DIV_CLK_CPUCL1_SHORTSTOP,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_htu[] = {
	DIV_CLK_CPUCL1_HTU,
};
enum clk_id cmucal_vclk_div_clk_dsu_shortstop[] = {
	DIV_CLK_DSU_SHORTSTOP,
};
enum clk_id cmucal_vclk_div_clk_cluster0_atclk[] = {
	DIV_CLK_CLUSTER0_ATCLK,
};
enum clk_id cmucal_vclk_div_clk_peri_usi00_usi[] = {
	DIV_CLK_PERI_USI00_USI,
	MUX_CLKCMU_PERI_USI00_USI_USER,
};
enum clk_id cmucal_vclk_mux_clkcmu_peri_ip[] = {
	MUX_CLKCMU_PERI_IP,
};
enum clk_id cmucal_vclk_div_clk_peri_usi01_usi[] = {
	DIV_CLK_PERI_USI01_USI,
	MUX_CLKCMU_PERI_USI01_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi02_usi[] = {
	DIV_CLK_PERI_USI02_USI,
	MUX_CLKCMU_PERI_USI02_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi03_usi[] = {
	DIV_CLK_PERI_USI03_USI,
	MUX_CLKCMU_PERI_USI03_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi04_usi[] = {
	DIV_CLK_PERI_USI04_USI,
	MUX_CLKCMU_PERI_USI04_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_usi05_usi[] = {
	DIV_CLK_PERI_USI05_USI,
	MUX_CLKCMU_PERI_USI05_USI_USER,
};
enum clk_id cmucal_vclk_div_clk_peri_uart_dbg[] = {
	DIV_CLK_PERI_UART_DBG,
};
enum clk_id cmucal_vclk_div_clk_peri_usi06_usi[] = {
	DIV_CLK_PERI_USI06_USI,
	MUX_CLKCMU_PERI_USI06_USI_USER,
};
enum clk_id cmucal_vclk_div_vts_serial_lif_core[] = {
	DIV_VTS_SERIAL_LIF_CORE,
	DIV_VTS_SERIAL_LIF,
	MUX_VTS_SERIAL_LIF,
};
enum clk_id cmucal_vclk_clkcmu_chubvts_bus[] = {
	CLKCMU_CHUBVTS_BUS,
	MUX_CLKCMU_CHUBVTS_BUS,
};
enum clk_id cmucal_vclk_mux_clkcmu_ap2gnss[] = {
	MUX_CLKCMU_AP2GNSS,
};
/* COMMON VCLK -> Clock Node List */
enum clk_id cmucal_vclk_blk_cmu[] = {
	MUX_CLKCMU_CORE_G3D,
	CLKCMU_G3D_BUS,
	CLKCMU_USB_BUS,
	MUX_CLKCMU_USB_BUS,
	MUX_CLKCMU_MIF_BUSP,
	MUX_CLKCMU_HSI_UFS_EMBD,
	MUX_CLKCMU_CORE_SSS,
	PLL_SHARED1,
	PLL_SHARED2,
};
enum clk_id cmucal_vclk_blk_s2d[] = {
	MUX_CLK_S2D_CORE,
	PLL_MIF_S2D,
};
enum clk_id cmucal_vclk_blk_alive[] = {
	CLKCMU_CMGP_BUS,
	MUX_CLKCMU_CMGP_BUS,
	DIV_CLK_ALIVE_BUS,
	MUX_CLK_ALIVE_BUS,
};
enum clk_id cmucal_vclk_blk_aud[] = {
	DIV_CLK_AUD_BUSP,
	DIV_CLK_AUD_BUSD,
	MUX_CLK_AUD_BUS,
	DIV_CLK_AUD_CPU_ACLK,
};
enum clk_id cmucal_vclk_blk_chub[] = {
	DIV_CLK_CHUB_I2C,
	MUX_CLK_CHUB_I2C,
	DIV_CLK_CHUB_BUS,
	MUX_CLK_CHUB_BUS,
};
enum clk_id cmucal_vclk_blk_chubvts[] = {
	DIV_CLK_CHUBVTS_BUS,
	MUX_CLK_CHUBVTS_BUS,
};
enum clk_id cmucal_vclk_blk_cmgp[] = {
	DIV_CLK_CMGP_I2C,
	MUX_CLK_CMGP_I2C,
	MUX_CLK_CMGP_BUS,
};
enum clk_id cmucal_vclk_blk_core[] = {
	MUX_CLK_CORE_GIC,
	DIV_CLK_CORE_BUSP,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	MUX_CLK_CPUCL0_PLL,
};
enum clk_id cmucal_vclk_blk_cpucl1[] = {
	MUX_CLK_CPUCL1_PLL,
};
enum clk_id cmucal_vclk_blk_dsu[] = {
	DIV_CLK_CLUSTER0_PCLK,
	DIV_CLK_CLUSTER0_PERIPHCLK,
	MUX_CLK_DSU_PLL,
};
enum clk_id cmucal_vclk_blk_usb[] = {
	MUX_CLK_USB_BUS,
};
enum clk_id cmucal_vclk_blk_vts[] = {
	DIV_CLK_VTS_BUS,
	MUX_CLK_VTS_BUS,
	DIV_VTS_DMIC_AUD_DIV2,
	DIV_VTS_DMIC_AUD,
	MUX_VTS_DMIC_AUD,
	DIV_CLK_VTS_DMIC_IF_DIV2,
	DIV_CLK_VTS_DMIC_IF,
	MUX_CLK_VTS_DMIC_IF,
};
enum clk_id cmucal_vclk_blk_busc[] = {
	DIV_CLK_BUSC_BUSP,
};
enum clk_id cmucal_vclk_blk_cpucl0_glb[] = {
	DIV_CLK_CPUCL0_DBG_PCLKDBG,
};
enum clk_id cmucal_vclk_blk_csis[] = {
	DIV_CLK_CSIS_BUSP,
};
enum clk_id cmucal_vclk_blk_dpu[] = {
	DIV_CLK_DPU_BUSP,
};
enum clk_id cmucal_vclk_blk_g3d[] = {
	DIV_CLK_G3D_BUSP,
};
enum clk_id cmucal_vclk_blk_isp[] = {
	DIV_CLK_ISP_BUSP,
};
enum clk_id cmucal_vclk_blk_m2m[] = {
	DIV_CLK_M2M_BUSP,
};
enum clk_id cmucal_vclk_blk_mcsc[] = {
	DIV_CLK_MCSC_BUSP,
};
enum clk_id cmucal_vclk_blk_mfc[] = {
	DIV_CLK_MFC_BUSP,
};
enum clk_id cmucal_vclk_blk_npu0[] = {
	DIV_CLK_NPU0_BUSP,
};
enum clk_id cmucal_vclk_blk_npus[] = {
	DIV_CLK_NPUS_BUSP,
};
enum clk_id cmucal_vclk_blk_peri[] = {
	DIV_CLK_PERI_USI_I2C,
};
enum clk_id cmucal_vclk_blk_taa[] = {
	DIV_CLK_TAA_BUSP,
};
enum clk_id cmucal_vclk_blk_tnr[] = {
	DIV_CLK_TNR_BUSP,
};
/* GATE VCLK -> Clock Node List */
enum clk_id cmucal_vclk_ip_slh_axi_si_d_apm[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_SI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_apm[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_MI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_wdt_alive[] = {
	GOUT_BLK_ALIVE_UID_WDT_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_alive[] = {
	GOUT_BLK_ALIVE_UID_SYSREG_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_ap[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_alive[] = {
	GOUT_BLK_ALIVE_UID_APBIF_PMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_intmem[] = {
	GOUT_BLK_ALIVE_UID_INTMEM_IPCLKPORT_ACLK,
	GOUT_BLK_ALIVE_UID_INTMEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_g_scan2dram[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_SI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pmu_intr_gen[] = {
	GOUT_BLK_ALIVE_UID_PMU_INTR_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_alive[] = {
	GOUT_BLK_ALIVE_UID_XIU_DP_ALIVE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_alive_cmu_alive[] = {
	CLK_BLK_ALIVE_UID_ALIVE_CMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration[] = {
	GOUT_BLK_ALIVE_UID_GREBEINTEGRATION_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_alive[] = {
	GOUT_BLK_ALIVE_UID_APBIF_GPIO_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_top_rtc[] = {
	GOUT_BLK_ALIVE_UID_APBIF_TOP_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ss_dbgcore[] = {
	GOUT_BLK_ALIVE_UID_SS_DBGCORE_IPCLKPORT_SS_DBGCORE_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_alive[] = {
	GOUT_BLK_ALIVE_UID_D_TZPC_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_vts[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_dbgcore[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_g_dbgcore[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_SI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_rtc[] = {
	GOUT_BLK_ALIVE_UID_APBIF_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_c_cmgp[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_SI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_alive[] = {
	GOUT_BLK_ALIVE_UID_VGEN_LITE_ALIVE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dbgcore_uart[] = {
	GOUT_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_PCLK,
	CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_rom_crc32_host[] = {
	GOUT_BLK_ALIVE_UID_ROM_CRC32_HOST_IPCLKPORT_PCLK,
	GOUT_BLK_ALIVE_UID_ROM_CRC32_HOST_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_gnss[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_MI_C_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_modem[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_MI_C_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_chubvts[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_MI_C_CHUBVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_wlbt[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_MI_C_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_lp_chubvts[] = {
	GOUT_BLK_ALIVE_UID_SLH_AXI_SI_LP_CHUBVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_chub[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_chub[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_WLBT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_wlbt_abox[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_WLBT_ABOX_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_wlbt_wl[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_WLBT_WL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_wlbt[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_wlbt[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_GNSS_WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_chub[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_GNSS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_gnss[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_gnss[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_gnss[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_CP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_wlbt_wl[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_CP_WLBT_WL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_chub[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_CP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp_s[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_CP_S_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_cp[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_vts_chub[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_VTS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_chub[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i3c_apm_pmic[] = {
	GOUT_BLK_ALIVE_UID_I3C_APM_PMIC_IPCLKPORT_I_PCLK,
	GOUT_BLK_ALIVE_UID_I3C_APM_PMIC_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_apbif_sysreg_vgpio2ap[] = {
	GOUT_BLK_ALIVE_UID_APBIF_SYSREG_VGPIO2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_sysreg_vgpio2apm[] = {
	GOUT_BLK_ALIVE_UID_APBIF_SYSREG_VGPIO2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_sysreg_vgpio2pmu[] = {
	GOUT_BLK_ALIVE_UID_APBIF_SYSREG_VGPIO2PMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_chub_rtc[] = {
	GOUT_BLK_ALIVE_UID_APBIF_CHUB_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_wlbt_bt[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_WLBT_BT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_wlbt_bt[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_CP_WLBT_BT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hw_scandump_clkstop_ctrl[] = {
	GOUT_BLK_ALIVE_UID_HW_SCANDUMP_CLKSTOP_CTRL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sweeper_p_alive[] = {
	GOUT_BLK_ALIVE_UID_SWEEPER_P_ALIVE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_i2c_alive0[] = {
	GOUT_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_IPCLK,
	GOUT_BLK_ALIVE_UID_I2C_ALIVE0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_alive0[] = {
	GOUT_BLK_ALIVE_UID_USI_ALIVE0_IPCLKPORT_IPCLK,
	GOUT_BLK_ALIVE_UID_USI_ALIVE0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_shared_sram[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_SHARED_SRAM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_aud_cmu_aud[] = {
	CLK_BLK_AUD_UID_AUD_CMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_aud[] = {
	GOUT_BLK_AUD_UID_LH_AXI_SI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_aud[] = {
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_abox[] = {
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF0,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF1,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF3,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_DAP,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_CNT,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF4,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_CA32,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_DSIF,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF5,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF6,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_PCMC_CLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_C2A0_CLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_C2A1_CLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_XCLK_0,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_XCLK_1,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_XCLK_2,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_ACP,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_OSC_SPDY,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_SPDY,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_aud[] = {
	GOUT_BLK_AUD_UID_SLH_AXI_MI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_peri_axi_asb[] = {
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_PCLK,
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_wdt_aud[] = {
	GOUT_BLK_AUD_UID_WDT_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_aud[] = {
	GOUT_BLK_AUD_UID_SYSMMU_AUD_IPCLKPORT_CLK_S1,
	GOUT_BLK_AUD_UID_SYSMMU_AUD_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud_ns[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud_s[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_aud_s2[] = {
	GOUT_BLK_AUD_UID_AD_APB_SYSMMU_AUD_S2_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dftmux_aud[] = {
	GOUT_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_AUD_CODEC_MCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud0[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud1[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_aud[] = {
	GOUT_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_usbaud[] = {
	GOUT_BLK_AUD_UID_SLH_AXI_MI_D_USBAUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_aud[] = {
	GOUT_BLK_AUD_UID_VGEN_LITE_AUD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgenlite_aud[] = {
	GOUT_BLK_AUD_UID_AD_APB_VGENLITE_AUD_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_aud[] = {
	GOUT_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busc_cmu_busc[] = {
	CLK_BLK_BUSC_UID_BUSC_CMU_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma[] = {
	GOUT_BLK_BUSC_UID_AD_APB_PDMA_IPCLKPORT_PCLKM,
	GOUT_BLK_BUSC_UID_AD_APB_PDMA_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_xiu_p_busc[] = {
	GOUT_BLK_BUSC_UID_XIU_P_BUSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_busc[] = {
	GOUT_BLK_BUSC_UID_XIU_D_BUSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_peri[] = {
	GOUT_BLK_BUSC_UID_SLH_AXI_MI_D_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_usb[] = {
	GOUT_BLK_BUSC_UID_SLH_AXI_MI_D_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_mfc[] = {
	GOUT_BLK_BUSC_UID_LH_AXI_MI_D_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_apm[] = {
	GOUT_BLK_BUSC_UID_SLH_AXI_MI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_busc[] = {
	GOUT_BLK_BUSC_UID_PDMA_BUSC_IPCLKPORT_ACLK_PDMA0,
};
enum clk_id cmucal_vclk_ip_spdma_busc[] = {
	GOUT_BLK_BUSC_UID_SPDMA_BUSC_IPCLKPORT_ACLK_PDMA1,
};
enum clk_id cmucal_vclk_ip_sysmmu_axi_d_busc[] = {
	GOUT_BLK_BUSC_UID_SYSMMU_AXI_D_BUSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_busc[] = {
	GOUT_BLK_BUSC_UID_SYSREG_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_busc[] = {
	GOUT_BLK_BUSC_UID_TREX_D_BUSC_IPCLKPORT_ACLK,
	GOUT_BLK_BUSC_UID_TREX_D_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_pdma[] = {
	GOUT_BLK_BUSC_UID_VGEN_PDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_busc[] = {
	GOUT_BLK_BUSC_UID_SLH_AXI_MI_P_BUSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_busc[] = {
	GOUT_BLK_BUSC_UID_D_TZPC_BUSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_chubvts[] = {
	GOUT_BLK_BUSC_UID_LH_AXI_MI_D_CHUBVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_spdma[] = {
	GOUT_BLK_BUSC_UID_VGEN_SPDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_chub[] = {
	GOUT_BLK_CHUB_UID_APBIF_GPIO_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_chub_combine_wakeup_src[] = {
	GOUT_BLK_CHUB_UID_APBIF_CHUB_COMBINE_WAKEUP_SRC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cm4_chub[] = {
	GOUT_BLK_CHUB_UID_CM4_CHUB_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_pwm_chub[] = {
	GOUT_BLK_CHUB_UID_PWM_CHUB_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_sysreg_chub[] = {
	GOUT_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer_chub[] = {
	GOUT_BLK_CHUB_UID_TIMER_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_chub[] = {
	GOUT_BLK_CHUB_UID_WDT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub1[] = {
	GOUT_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_I2C_CHUB1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub0[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub1[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub2[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2ap[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2apm[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2wlbt[] = {
	GOUT_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_chubeint[] = {
	GOUT_BLK_CHUB_UID_APBIF_GPIO_CHUBEINT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub3[] = {
	GOUT_BLK_CHUB_UID_I2C_CHUB3_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_I2C_CHUB3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub3[] = {
	GOUT_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK,
	GOUT_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix_chub[] = {
	GOUT_BLK_CHUB_UID_AHB_BUSMATRIX_CHUB_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_m_chub[] = {
	GOUT_BLK_CHUB_UID_SLH_AXI_SI_M_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_s_chub[] = {
	GOUT_BLK_CHUB_UID_SLH_AXI_MI_S_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_chub_cmu_chub[] = {
	CLK_BLK_CHUB_UID_CHUB_CMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_chubvts_cmu_chubvts[] = {
	CLK_BLK_CHUBVTS_UID_CHUBVTS_CMU_CHUBVTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_vts[] = {
	GOUT_BLK_CHUBVTS_UID_BAAW_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_D_TZPC_CHUBVTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_LH_AXI_SI_D_CHUBVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_m_chub[] = {
	GOUT_BLK_CHUBVTS_UID_SLH_AXI_MI_M_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_lp_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_SLH_AXI_MI_LP_CHUBVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_m_vts[] = {
	GOUT_BLK_CHUBVTS_UID_SLH_AXI_MI_M_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_c_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_SLH_AXI_SI_C_CHUBVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_s_chub[] = {
	GOUT_BLK_CHUBVTS_UID_SLH_AXI_SI_S_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_s_vts[] = {
	GOUT_BLK_CHUBVTS_UID_SLH_AXI_SI_S_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sweeper_c_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_SWEEPER_C_CHUBVTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_SYSREG_CHUBVTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_VGEN_LITE_CHUBVTS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_XIU_DP_CHUBVTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_bps_lp_chubvts[] = {
	GOUT_BLK_CHUBVTS_UID_BPS_LP_CHUBVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_chub[] = {
	GOUT_BLK_CHUBVTS_UID_BAAW_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_cmgp[] = {
	GOUT_BLK_CMGP_UID_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp0[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp0[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_ap[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cmgp[] = {
	GOUT_BLK_CMGP_UID_D_TZPC_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_cmgp[] = {
	GOUT_BLK_CMGP_UID_SLH_AXI_MI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2apm[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2cp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i3c_cmgp[] = {
	GOUT_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_SCLK,
	GOUT_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2chub[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2gnss[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2wlbt[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2WLBT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp4[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP4_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp4[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp1[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp2[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp3[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp1[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp2[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp3[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_core_cmu_core[] = {
	CLK_BLK_CORE_UID_CORE_CMU_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_core[] = {
	GOUT_BLK_CORE_UID_SYSREG_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sirex[] = {
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_I_ACLK,
	GOUT_BLK_CORE_UID_SIREX_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p_core[] = {
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_ACLK_P_CORE,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_P_CORE_IPCLKPORT_PCLK_P_CORE,
};
enum clk_id cmucal_vclk_ip_dit[] = {
	GOUT_BLK_CORE_UID_DIT_IPCLKPORT_ICLKL2A,
};
enum clk_id cmucal_vclk_ip_trex_d_nrt[] = {
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_D_NRT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_g3d[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_cpucl0[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_csis[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_hsi[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_MI_D_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_d_core[] = {
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_GCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_apm[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_core[] = {
	GOUT_BLK_CORE_UID_D_TZPC_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_wlbt[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_MI_D_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_wlbt[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_WLBT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d0_modem[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_MI_D0_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d1_modem[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_MI_D1_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_gnss[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_MI_D_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LH_AXI_SI_D0_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dit[] = {
	GOUT_BLK_CORE_UID_AD_APB_DIT_IPCLKPORT_PCLKS,
	GOUT_BLK_CORE_UID_AD_APB_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_gic[] = {
	GOUT_BLK_CORE_UID_AD_AXI_GIC_IPCLKPORT_ACLKS,
	GOUT_BLK_CORE_UID_AD_AXI_GIC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_usb[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_modem[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mif0[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mif1[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mfc[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_gnss[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gic[] = {
	GOUT_BLK_CORE_UID_GIC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_aud[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mif_cp[] = {
	GOUT_BLK_CORE_UID_LH_AXI_SI_D1_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mif_nrt[] = {
	GOUT_BLK_CORE_UID_LH_AXI_SI_D1_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_aud[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_dpu[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_hsi[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_taa[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_gnss[] = {
	GOUT_BLK_CORE_UID_BAAW_P_GNSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_modem[] = {
	GOUT_BLK_CORE_UID_BAAW_P_MODEM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_wlbt[] = {
	GOUT_BLK_CORE_UID_BAAW_P_WLBT_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sfr_apbif_cmu_topc[] = {
	GOUT_BLK_CORE_UID_SFR_APBIF_CMU_TOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_isp[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mcsc[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_tnr[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_npu0[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_peri[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_sss[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D_SSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_acel_d_dit[] = {
	GOUT_BLK_CORE_UID_SYSMMU_ACEL_D_DIT_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_sss[] = {
	GOUT_BLK_CORE_UID_LH_AXI_SI_D_SSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_g_cssys[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_MI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_mif_cp[] = {
	GOUT_BLK_CORE_UID_LH_AXI_SI_D0_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_I_PCLK,
	GOUT_BLK_CORE_UID_SSS_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_puf[] = {
	GOUT_BLK_CORE_UID_PUF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_puf[] = {
	GOUT_BLK_CORE_UID_AD_APB_PUF_IPCLKPORT_PCLKS,
	GOUT_BLK_CORE_UID_AD_APB_PUF_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_npus[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_mif_rt[] = {
	GOUT_BLK_CORE_UID_LH_AXI_SI_D0_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mif_rt[] = {
	GOUT_BLK_CORE_UID_LH_AXI_SI_D1_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_busc[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_BUSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mcw[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_MCW_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_g3d[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_sss[] = {
	GOUT_BLK_CORE_UID_BAAW_D_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bdu[] = {
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_CLK,
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_g_bdu[] = {
	GOUT_BLK_CORE_UID_XIU_G_BDU_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppc_debug[] = {
	GOUT_BLK_CORE_UID_PPC_DEBUG_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_dpu[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D0_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_npus[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D0_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_npus[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D1_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_acel_d2_modem[] = {
	GOUT_BLK_CORE_UID_SYSMMU_ACEL_D2_MODEM_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_bdu[] = {
	GOUT_BLK_CORE_UID_ADM_APB_G_BDU_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_dpu[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D1_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_m2m[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_SI_P_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g_cpu[] = {
	GOUT_BLK_CORE_UID_LH_AST_MI_G_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_cluster0[] = {
	GOUT_BLK_CORE_UID_SLH_AXI_MI_P_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_core[] = {
	GOUT_BLK_CORE_UID_VGEN_LITE_CORE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_lite_core[] = {
	GOUT_BLK_CORE_UID_AD_APB_VGEN_LITE_CORE_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_m2m[] = {
	GOUT_BLK_CORE_UID_LH_AXI_MI_D_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hw_apbsema_mec[] = {
	GOUT_BLK_CORE_UID_HW_APBSEMA_MEC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_HTU_CPUCL0_IPCLKPORT_I_CLK,
	GOUT_BLK_CPUCL0_UID_HTU_CPUCL0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl0[] = {
	GOUT_BLK_CPUCL0_UID_CPUCL0_IPCLKPORT_CORECLK_AN,
};
enum clk_id cmucal_vclk_ip_cpucl0_glb_cmu_cpucl0_glb[] = {
	CLK_BLK_CPUCL0_GLB_UID_CPUCL0_GLB_CMU_CPUCL0_GLB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apb_async_p_cssys_0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_APB_ASYNC_P_CSSYS_0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_xiu_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_XIU_P_CPUCL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_bps_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_BPS_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_CSSYS_IPCLKPORT_PCLKDBG,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_D_TZPC_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_g_dbgcore[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_g_int_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_G_INT_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_g_int_dbgcore[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_G_INT_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_g_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SLH_AXI_SI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_g_int_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SLH_AXI_SI_G_INT_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_g_int_dbgcore[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SLH_AXI_SI_G_INT_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_secjtag[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SECJTAG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_XIU_DP_CSSYS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cluster0[] = {
	CLK_BLK_CPUCL0_GLB_UID_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_cpucl1_cmu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_CMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_CORECLK_HC,
};
enum clk_id cmucal_vclk_ip_htu_cpucl1[] = {
	GOUT_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK,
	GOUT_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_csis_cmu_csis[] = {
	CLK_BLK_CSIS_UID_CSIS_CMU_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_csis[] = {
	GOUT_BLK_CSIS_UID_LH_AXI_SI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_csis[] = {
	GOUT_BLK_CSIS_UID_LH_AXI_SI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_csis[] = {
	GOUT_BLK_CSIS_UID_D_TZPC_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_csis_pdp[] = {
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_MCB,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_I_PDP_CLK,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_VOTF0,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_VOTF1,
	GOUT_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_I_PDP_C2CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_csis_d0[] = {
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_csis_d1[] = {
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_csis[] = {
	GOUT_BLK_CSIS_UID_SYSREG_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_csis[] = {
	GOUT_BLK_CSIS_UID_SLH_AXI_MI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf0_csistaa[] = {
	GOUT_BLK_CSIS_UID_LH_AST_SI_OTF0_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_zotf0_taacsis[] = {
	GOUT_BLK_CSIS_UID_LH_AST_MI_ZOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_zotf1_taacsis[] = {
	GOUT_BLK_CSIS_UID_LH_AST_MI_ZOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis0[] = {
	GOUT_BLK_CSIS_UID_AD_APB_CSIS0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_xiu_d0_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D0_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D1_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf1_csistaa[] = {
	GOUT_BLK_CSIS_UID_LH_AST_SI_OTF1_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_sotf0_taacsis[] = {
	GOUT_BLK_CSIS_UID_LH_AST_MI_SOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_sotf1_taacsis[] = {
	GOUT_BLK_CSIS_UID_LH_AST_MI_SOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_csis_d2[] = {
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D2_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma0[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma1[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA1_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl0[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_ZSL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_strp0[] = {
	GOUT_BLK_CSIS_UID_QE_STRP0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_STRP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat_img0[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mipi_dcphy_link_wrap[] = {
	GOUT_BLK_CSIS_UID_MIPI_DCPHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS0,
	GOUT_BLK_CSIS_UID_MIPI_DCPHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS1,
	GOUT_BLK_CSIS_UID_MIPI_DCPHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS2,
	GOUT_BLK_CSIS_UID_MIPI_DCPHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS3,
	GOUT_BLK_CSIS_UID_MIPI_DCPHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS4,
	GOUT_BLK_CSIS_UID_MIPI_DCPHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS5,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d2_csis[] = {
	GOUT_BLK_CSIS_UID_LH_AXI_SI_D2_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl1[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_ZSL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_strp1[] = {
	GOUT_BLK_CSIS_UID_QE_STRP1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_STRP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat_img1[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma2[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma3[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA3_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_af0[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_AF0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_AF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_af1[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_AF1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_AF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d2_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D2_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d3_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D3_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D2_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D2_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d4_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D4_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_csis_d3[] = {
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D3_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_CSIS_D3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d3_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D3_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D3_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d3_csis[] = {
	GOUT_BLK_CSIS_UID_LH_AXI_SI_D3_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_af2[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_AF2_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_PDP_AF2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_strp2[] = {
	GOUT_BLK_CSIS_UID_QE_STRP2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_STRP2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl2[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_ZSL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat_img2[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf2_csistaa[] = {
	GOUT_BLK_CSIS_UID_LH_AST_SI_OTF2_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_zotf2_taacsis[] = {
	GOUT_BLK_CSIS_UID_LH_AST_MI_ZOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_sotf2_taacsis[] = {
	GOUT_BLK_CSIS_UID_LH_AST_MI_SOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite0_csis[] = {
	GOUT_BLK_CSIS_UID_VGEN_LITE0_CSIS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite1_csis[] = {
	GOUT_BLK_CSIS_UID_VGEN_LITE1_CSIS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite2_csis[] = {
	GOUT_BLK_CSIS_UID_VGEN_LITE2_CSIS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dpu_cmu_dpu[] = {
	CLK_BLK_DPU_UID_DPU_CMU_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dpu[] = {
	GOUT_BLK_DPU_UID_SYSREG_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_axi_d0_dpu[] = {
	GOUT_BLK_DPU_UID_SYSMMU_AXI_D0_DPU_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPU_UID_SYSMMU_AXI_D0_DPU_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_dpu[] = {
	GOUT_BLK_DPU_UID_SLH_AXI_MI_P_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_dpu[] = {
	GOUT_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_ACLK,
	GOUT_BLK_DPU_UID_PPMU_D0_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_dpu[] = {
	GOUT_BLK_DPU_UID_LH_AXI_SI_D0_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpu[] = {
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DECON,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_DPP,
	GOUT_BLK_DPU_UID_DPU_IPCLKPORT_ACLK_C2SERV,
	CLK_BLK_DPU_UID_DPU_IPCLKPORT_I_NEWCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpu[] = {
	GOUT_BLK_DPU_UID_D_TZPC_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_decon0[] = {
	GOUT_BLK_DPU_UID_AD_APB_DECON0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_dpu[] = {
	GOUT_BLK_DPU_UID_LH_AXI_SI_D1_DPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_dpu[] = {
	GOUT_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_ACLK,
	GOUT_BLK_DPU_UID_PPMU_D1_DPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_axi_d1_dpu[] = {
	GOUT_BLK_DPU_UID_SYSMMU_AXI_D1_DPU_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPU_UID_SYSMMU_AXI_D1_DPU_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_cluster0[] = {
	GOUT_BLK_DSU_UID_SLH_AXI_SI_P_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_cpucl0[] = {
	GOUT_BLK_DSU_UID_XIU_D_CPUCL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_dsu_cmu_dsu[] = {
	CLK_BLK_DSU_UID_DSU_CMU_DSU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_mif_cpu[] = {
	GOUT_BLK_DSU_UID_LH_AXI_SI_D0_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mif_cpu[] = {
	GOUT_BLK_DSU_UID_LH_AXI_SI_D1_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrrun_cluster0_0[] = {
	GOUT_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_0_IPCLKPORT_CLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrrun_cluster0_1[] = {
	GOUT_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_1_IPCLKPORT_CLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrret_cluster0_0[] = {
	GOUT_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_0_IPCLKPORT_CLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrret_cluster0_1[] = {
	GOUT_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_1_IPCLKPORT_CLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cluster0[] = {
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_ATCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_PCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_PERIPHCLK,
	GOUT_BLK_DSU_UID_CLUSTER0_IPCLKPORT_GICCLK,
};
enum clk_id cmucal_vclk_ip_htu_dsu[] = {
	GOUT_BLK_DSU_UID_HTU_DSU_IPCLKPORT_I_CLK,
	CLK_BLK_DSU_UID_HTU_DSU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl0[] = {
	GOUT_BLK_DSU_UID_PPMU_CPUCL0_IPCLKPORT_ACLK,
	GOUT_BLK_DSU_UID_PPMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl1[] = {
	GOUT_BLK_DSU_UID_PPMU_CPUCL1_IPCLKPORT_ACLK,
	GOUT_BLK_DSU_UID_PPMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_g_cpu[] = {
	GOUT_BLK_DSU_UID_LH_AST_SI_G_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_g3d[] = {
	GOUT_BLK_G3D_UID_SLH_AXI_MI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_g3d[] = {
	GOUT_BLK_G3D_UID_SYSREG_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_g3d_cmu_g3d[] = {
	CLK_BLK_G3D_UID_G3D_CMU_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpu[] = {
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK,
	GOUT_BLK_G3D_UID_GPU_IPCLKPORT_CLK_COREGROUP,
	GOUT_BLK_G3D_UID_GPU_IPCLKPORT_CLK_COREGROUP,
	GOUT_BLK_G3D_UID_GPU_IPCLKPORT_CLK_STACKS,
	GOUT_BLK_G3D_UID_GPU_IPCLKPORT_CLK_STACKS,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g3d[] = {
	GOUT_BLK_G3D_UID_D_TZPC_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_g3d[] = {
	GOUT_BLK_G3D_UID_LH_AXI_SI_D_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_htu_g3d[] = {
	GOUT_BLK_G3D_UID_HTU_G3D_IPCLKPORT_I_PCLK,
	GOUT_BLK_G3D_UID_HTU_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d_g3d[] = {
	GOUT_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_PCLK,
	GOUT_BLK_G3D_UID_PPMU_D_G3D_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_as_apb_sysmmu_d_g3d[] = {
	GOUT_BLK_G3D_UID_AS_APB_SYSMMU_D_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_g3d[] = {
	GOUT_BLK_G3D_UID_SYSMMU_D_G3D_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_gray2bin_g3d[] = {
	GOUT_BLK_G3D_UID_GRAY2BIN_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_g3d[] = {
	GOUT_BLK_G3D_UID_XIU_D0_G3D_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_as_apb_vgenlite_g3d[] = {
	GOUT_BLK_G3D_UID_AS_APB_VGENLITE_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_g3d[] = {
	GOUT_BLK_G3D_UID_VGEN_LITE_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_gnss_cmu_gnss[] = {
	CLK_BLK_GNSS_UID_GNSS_CMU_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi[] = {
	GOUT_BLK_HSI_UID_VGEN_LITE_HSI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_hsi_cmu_hsi[] = {
	GOUT_BLK_HSI_UID_HSI_CMU_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi[] = {
	GOUT_BLK_HSI_UID_SYSREG_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi[] = {
	GOUT_BLK_HSI_UID_GPIO_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d_hsi[] = {
	GOUT_BLK_HSI_UID_SLH_AXI_SI_D_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_hsi[] = {
	GOUT_BLK_HSI_UID_SLH_AXI_MI_P_HSI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi[] = {
	GOUT_BLK_HSI_UID_PPMU_HSI_IPCLKPORT_ACLK,
	GOUT_BLK_HSI_UID_PPMU_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi[] = {
	GOUT_BLK_HSI_UID_D_TZPC_HSI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufs_embd[] = {
	GOUT_BLK_HSI_UID_UFS_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI_UID_UFS_EMBD_IPCLKPORT_I_FMP_CLK,
	GOUT_BLK_HSI_UID_UFS_EMBD_IPCLKPORT_I_CLK_UNIPRO,
};
enum clk_id cmucal_vclk_ip_s2mpu_d_hsi[] = {
	GOUT_BLK_HSI_UID_S2MPU_D_HSI_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_gpio_hsi_ufs[] = {
	GOUT_BLK_HSI_UID_GPIO_HSI_UFS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_isp[] = {
	GOUT_BLK_ISP_UID_SYSREG_ISP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_isp_cmu_isp[] = {
	CLK_BLK_ISP_UID_ISP_CMU_ISP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_isp[] = {
	GOUT_BLK_ISP_UID_D_TZPC_ISP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_isp[] = {
	CLK_BLK_ISP_UID_PPMU_ISP_IPCLKPORT_PCLK,
	CLK_BLK_ISP_UID_PPMU_ISP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_isp[] = {
	CLK_BLK_ISP_UID_LH_AXI_SI_D_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_itp[] = {
	GOUT_BLK_ISP_UID_AD_APB_ITP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_itp_dns[] = {
	GOUT_BLK_ISP_UID_ITP_DNS_IPCLKPORT_I_ITP_CLK,
	GOUT_BLK_ISP_UID_ITP_DNS_IPCLKPORT_I_DNS_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_isp[] = {
	GOUT_BLK_ISP_UID_SYSMMU_D_ISP_IPCLKPORT_CLK_S1,
	GOUT_BLK_ISP_UID_SYSMMU_D_ISP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf_taaisp[] = {
	GOUT_BLK_ISP_UID_LH_AST_MI_OTF_TAAISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf_ispmcsc[] = {
	GOUT_BLK_ISP_UID_LH_AST_SI_OTF_ISPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf0_tnrisp[] = {
	GOUT_BLK_ISP_UID_LH_AST_MI_OTF0_TNRISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf1_tnrisp[] = {
	GOUT_BLK_ISP_UID_LH_AST_MI_OTF1_TNRISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_isp[] = {
	GOUT_BLK_ISP_UID_XIU_D_ISP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_lite_isp[] = {
	GOUT_BLK_ISP_UID_AD_APB_VGEN_LITE_ISP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_isp[] = {
	CLK_BLK_ISP_UID_VGEN_LITE_ISP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_isp[] = {
	GOUT_BLK_ISP_UID_SLH_AXI_MI_P_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_m2m[] = {
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_VOTF,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_VOTF,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_2X1,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_2X1,
};
enum clk_id cmucal_vclk_ip_m2m_cmu_m2m[] = {
	CLK_BLK_M2M_UID_M2M_CMU_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_m2m[] = {
	GOUT_BLK_M2M_UID_SYSREG_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_m2m[] = {
	GOUT_BLK_M2M_UID_SLH_AXI_MI_P_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_m2m[] = {
	GOUT_BLK_M2M_UID_SYSMMU_D_M2M_IPCLKPORT_CLK_S1,
	GOUT_BLK_M2M_UID_SYSMMU_D_M2M_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_d_m2m[] = {
	GOUT_BLK_M2M_UID_PPMU_D_M2M_IPCLKPORT_ACLK,
	GOUT_BLK_M2M_UID_PPMU_D_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_m2m[] = {
	GOUT_BLK_M2M_UID_XIU_D_M2M_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_as_apb_jpeg0[] = {
	GOUT_BLK_M2M_UID_AS_APB_JPEG0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_jpeg0[] = {
	GOUT_BLK_M2M_UID_JPEG0_IPCLKPORT_I_SMFC_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_m2m[] = {
	GOUT_BLK_M2M_UID_D_TZPC_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_m2m[] = {
	GOUT_BLK_M2M_UID_LH_AXI_SI_D_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_as_apb_vgen_lite_m2m[] = {
	GOUT_BLK_M2M_UID_AS_APB_VGEN_LITE_M2M_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_m2m[] = {
	GOUT_BLK_M2M_UID_VGEN_LITE_M2M_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mcsc_cmu_mcsc[] = {
	CLK_BLK_MCSC_UID_MCSC_CMU_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mcsc[] = {
	GOUT_BLK_MCSC_UID_SLH_AXI_MI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSREG_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mcsc[] = {
	GOUT_BLK_MCSC_UID_PPMU_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcsc[] = {
	GOUT_BLK_MCSC_UID_D_TZPC_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_gdc[] = {
	GOUT_BLK_MCSC_UID_AD_APB_GDC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_gdc[] = {
	GOUT_BLK_MCSC_UID_PPMU_GDC_IPCLKPORT_ACLK,
	GOUT_BLK_MCSC_UID_PPMU_GDC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_gdc[] = {
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mcsc[] = {
	GOUT_BLK_MCSC_UID_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_mcsc[] = {
	GOUT_BLK_MCSC_UID_AD_AXI_MCSC_IPCLKPORT_ACLKM,
	GOUT_BLK_MCSC_UID_AD_AXI_MCSC_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_ad_axi_gdc[] = {
	GOUT_BLK_MCSC_UID_AD_AXI_GDC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_trex_d_cam[] = {
	GOUT_BLK_MCSC_UID_TREX_D_CAM_IPCLKPORT_ACLK,
	GOUT_BLK_MCSC_UID_TREX_D_CAM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_csis[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_tnr[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D0_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_csis[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_tnr[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D1_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d2_csis[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D2_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d3_csis[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D3_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_isp[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D_ISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_taa[] = {
	GOUT_BLK_MCSC_UID_LH_AXI_MI_D_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf_ispmcsc[] = {
	GOUT_BLK_MCSC_UID_LH_AST_MI_OTF_ISPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcsc[] = {
	GOUT_BLK_MCSC_UID_AD_APB_MCSC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d_mcsc[] = {
	GOUT_BLK_MCSC_UID_XIU_D_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_orbmch[] = {
	GOUT_BLK_MCSC_UID_ORBMCH_IPCLKPORT_ACLK,
	GOUT_BLK_MCSC_UID_ORBMCH_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_d0_mcsc_ns[] = {
	GOUT_BLK_MCSC_UID_AD_APB_SYSMMU_D0_MCSC_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_mcsc[] = {
	GOUT_BLK_MCSC_UID_VGEN_LITE_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_gdc[] = {
	GOUT_BLK_MCSC_UID_VGEN_LITE_GDC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mfc_cmu_mfc[] = {
	CLK_BLK_MFC_UID_MFC_CMU_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc[] = {
	GOUT_BLK_MFC_UID_AS_APB_MFC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc[] = {
	GOUT_BLK_MFC_UID_SYSREG_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_mfc[] = {
	GOUT_BLK_MFC_UID_LH_AXI_SI_D_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mfc[] = {
	GOUT_BLK_MFC_UID_SLH_AXI_MI_P_MFC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc[] = {
	GOUT_BLK_MFC_UID_SYSMMU_MFC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MFC_UID_SYSMMU_MFC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc[] = {
	GOUT_BLK_MFC_UID_PPMU_MFC_IPCLKPORT_ACLK,
	GOUT_BLK_MFC_UID_PPMU_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mfc[] = {
	GOUT_BLK_MFC_UID_MFC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfc[] = {
	GOUT_BLK_MFC_UID_D_TZPC_MFC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_mfc[] = {
	GOUT_BLK_MFC_UID_VGEN_LITE_MFC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mif_cmu_mif[] = {
	CLK_BLK_MIF_UID_MIF_CMU_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_ddrphy[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif[] = {
	GOUT_BLK_MIF_UID_SYSREG_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mif[] = {
	GOUT_BLK_MIF_UID_SLH_AXI_MI_P_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
	CLK_BLK_MIF_UID_DMC_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_SECURE,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PPMPU,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK_PF,
};
enum clk_id cmucal_vclk_ip_ddrphy[] = {
	GOUT_BLK_MIF_UID_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif[] = {
	GOUT_BLK_MIF_UID_D_TZPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_dmc_cpu[] = {
	GOUT_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_PCLK,
	GOUT_BLK_MIF_UID_QE_DMC_CPU_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dmc_cpu[] = {
	GOUT_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_ACLK,
	GOUT_BLK_MIF_UID_PPMU_DMC_CPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_pf[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_PF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_secure[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_SECURE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sfrapb_bridge_dmc_ppmpu[] = {
	GOUT_BLK_MIF_UID_SFRAPB_BRIDGE_DMC_PPMPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_mif_cpu[] = {
	GOUT_BLK_MIF_UID_LH_AXI_MI_D_MIF_CPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_mif_rt[] = {
	GOUT_BLK_MIF_UID_LH_AXI_MI_D_MIF_RT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_mif_nrt[] = {
	GOUT_BLK_MIF_UID_LH_AXI_MI_D_MIF_NRT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_mif_cp[] = {
	GOUT_BLK_MIF_UID_LH_AXI_MI_D_MIF_CP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_modem_cmu_modem[] = {
	CLK_BLK_MODEM_UID_MODEM_CMU_MODEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_npu0_cmu_npu0[] = {
	CLK_BLK_NPU0_UID_NPU0_CMU_NPU0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_npu0[] = {
	GOUT_BLK_NPU0_UID_D_TZPC_NPU0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_npu0[] = {
	GOUT_BLK_NPU0_UID_SYSREG_NPU0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_npu0[] = {
	GOUT_BLK_NPU0_UID_SLH_AXI_MI_P_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_npucore[] = {
	CLK_BLK_NPU0_UID_IP_NPUCORE_IPCLKPORT_I_PCLK,
	CLK_BLK_NPU0_UID_IP_NPUCORE_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_ctrl_npu0[] = {
	GOUT_BLK_NPU0_UID_LH_AXI_MI_D_CTRL_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_npu0[] = {
	GOUT_BLK_NPU0_UID_LH_AXI_MI_D0_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_rq_npu0[] = {
	GOUT_BLK_NPU0_UID_LH_AXI_SI_D_RQ_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_cmdq_npu0[] = {
	GOUT_BLK_NPU0_UID_LH_AXI_SI_D_CMDQ_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_npu0[] = {
	GOUT_BLK_NPU0_UID_LH_AXI_MI_D1_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_npus[] = {
	GOUT_BLK_NPUS_UID_VGEN_LITE_NPUS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_rq_npu0[] = {
	GOUT_BLK_NPUS_UID_LH_AXI_MI_D_RQ_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_ctrl_npu0[] = {
	GOUT_BLK_NPUS_UID_LH_AXI_SI_D_CTRL_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_npu0[] = {
	GOUT_BLK_NPUS_UID_LH_AXI_SI_D0_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_npus[] = {
	GOUT_BLK_NPUS_UID_LH_AXI_SI_D0_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_npus[] = {
	GOUT_BLK_NPUS_UID_LH_AXI_SI_D1_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_npus[] = {
	GOUT_BLK_NPUS_UID_SYSMMU_D0_NPUS_IPCLKPORT_CLK_S1,
	GOUT_BLK_NPUS_UID_SYSMMU_D0_NPUS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ip_npus[] = {
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_DBGCLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_C2A0CLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_C2A1CLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_ACLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_npus_0[] = {
	GOUT_BLK_NPUS_UID_PPMU_NPUS_0_IPCLKPORT_ACLK,
	GOUT_BLK_NPUS_UID_PPMU_NPUS_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_npus[] = {
	CLK_BLK_NPUS_UID_HTU_NPUS_IPCLKPORT_I_CLK,
	CLK_BLK_NPUS_UID_HTU_NPUS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_cmdq_npu0[] = {
	GOUT_BLK_NPUS_UID_LH_AXI_MI_D_CMDQ_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_npus[] = {
	GOUT_BLK_NPUS_UID_SYSMMU_D1_NPUS_IPCLKPORT_CLK_S1,
	GOUT_BLK_NPUS_UID_SYSMMU_D1_NPUS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_npu0[] = {
	GOUT_BLK_NPUS_UID_LH_AXI_SI_D1_NPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_npus_1[] = {
	GOUT_BLK_NPUS_UID_PPMU_NPUS_1_IPCLKPORT_ACLK,
	GOUT_BLK_NPUS_UID_PPMU_NPUS_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ds_256_128_0[] = {
	GOUT_BLK_NPUS_UID_DS_256_128_0_IPCLKPORT_MAINCLK,
};
enum clk_id cmucal_vclk_ip_ds_256_128_1[] = {
	GOUT_BLK_NPUS_UID_DS_256_128_1_IPCLKPORT_MAINCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_npus[] = {
	CLK_BLK_NPUS_UID_D_TZPC_NPUS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_int_npus[] = {
	GOUT_BLK_NPUS_UID_SLH_AXI_MI_P_INT_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_int_npus[] = {
	GOUT_BLK_NPUS_UID_SLH_AXI_SI_P_INT_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_d0_npus_ns[] = {
	GOUT_BLK_NPUS_UID_AD_APB_SYSMMU_D0_NPUS_NS_IPCLKPORT_PCLKS,
	GOUT_BLK_NPUS_UID_AD_APB_SYSMMU_D0_NPUS_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_npus[] = {
	CLK_BLK_NPUS_UID_SYSREG_NPUS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_npus[] = {
	GOUT_BLK_NPUS_UID_SLH_AXI_MI_P_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_adm_dap_npus[] = {
	GOUT_BLK_NPUS_UID_ADM_DAP_NPUS_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_npus_cmu_npus[] = {
	CLK_BLK_NPUS_UID_NPUS_CMU_NPUS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peri[] = {
	GOUT_BLK_PERI_UID_GPIO_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peri[] = {
	GOUT_BLK_PERI_UID_SYSREG_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peri_cmu_peri[] = {
	CLK_BLK_PERI_UID_PERI_CMU_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_peri[] = {
	GOUT_BLK_PERI_UID_SLH_AXI_MI_P_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peri[] = {
	GOUT_BLK_PERI_UID_D_TZPC_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_peri[] = {
	GOUT_BLK_PERI_UID_XIU_P_PERI_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	GOUT_BLK_PERI_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	GOUT_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
	CLK_BLK_PERI_UID_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_wdt0[] = {
	GOUT_BLK_PERI_UID_WDT0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt1[] = {
	GOUT_BLK_PERI_UID_WDT1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu[] = {
	GOUT_BLK_PERI_UID_TMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pwm[] = {
	GOUT_BLK_PERI_UID_PWM_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_uart_dbg[] = {
	GOUT_BLK_PERI_UID_UART_DBG_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_UART_DBG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_usi[] = {
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_i2c[] = {
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI00_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi01_usi[] = {
	GOUT_BLK_PERI_UID_USI01_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI01_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi01_i2c[] = {
	GOUT_BLK_PERI_UID_USI01_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI01_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi02_usi[] = {
	GOUT_BLK_PERI_UID_USI02_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI02_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi02_i2c[] = {
	GOUT_BLK_PERI_UID_USI02_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI02_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi03_usi[] = {
	GOUT_BLK_PERI_UID_USI03_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI03_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi03_i2c[] = {
	GOUT_BLK_PERI_UID_USI03_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI03_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi04_usi[] = {
	GOUT_BLK_PERI_UID_USI04_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI04_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi04_i2c[] = {
	GOUT_BLK_PERI_UID_USI04_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI04_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi05_usi[] = {
	GOUT_BLK_PERI_UID_USI05_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI05_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi05_i2c[] = {
	GOUT_BLK_PERI_UID_USI05_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI05_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_peri[] = {
	GOUT_BLK_PERI_UID_VGEN_LITE_PERI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_s2mpu_d_peri[] = {
	GOUT_BLK_PERI_UID_S2MPU_D_PERI_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	GOUT_BLK_PERI_UID_MMC_CARD_IPCLKPORT_I_ACLK,
	GOUT_BLK_PERI_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_ppmu_peri[] = {
	GOUT_BLK_PERI_UID_PPMU_PERI_IPCLKPORT_ACLK,
	GOUT_BLK_PERI_UID_PPMU_PERI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d_peri[] = {
	GOUT_BLK_PERI_UID_SLH_AXI_SI_D_PERI_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gpio_perimmc[] = {
	GOUT_BLK_PERI_UID_GPIO_PERIMMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi06_usi[] = {
	GOUT_BLK_PERI_UID_USI06_USI_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI06_USI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi06_i2c[] = {
	GOUT_BLK_PERI_UID_USI06_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI06_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi07_i2c[] = {
	GOUT_BLK_PERI_UID_USI07_I2C_IPCLKPORT_IPCLK,
	GOUT_BLK_PERI_UID_USI07_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_s2d_cmu_s2d[] = {
	CLK_BLK_S2D_UID_S2D_CMU_S2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_bis_s2d[] = {
	GOUT_BLK_S2D_UID_BIS_S2D_IPCLKPORT_CLK,
	CLK_BLK_S2D_UID_BIS_S2D_IPCLKPORT_SCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_g_scan2dram[] = {
	GOUT_BLK_S2D_UID_SLH_AXI_MI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_taa[] = {
	GOUT_BLK_TAA_UID_LH_AXI_SI_D_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_taa[] = {
	GOUT_BLK_TAA_UID_SLH_AXI_MI_P_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_taa[] = {
	GOUT_BLK_TAA_UID_SYSREG_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_taa_cmu_taa[] = {
	CLK_BLK_TAA_UID_TAA_CMU_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf_taaisp[] = {
	GOUT_BLK_TAA_UID_LH_AST_SI_OTF_TAAISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_taa[] = {
	GOUT_BLK_TAA_UID_D_TZPC_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf0_csistaa[] = {
	GOUT_BLK_TAA_UID_LH_AST_MI_OTF0_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sipu_taa[] = {
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_STAT,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_STAT,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_YDS,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_YDS,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_zotf0_taacsis[] = {
	GOUT_BLK_TAA_UID_LH_AST_SI_ZOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_zotf1_taacsis[] = {
	GOUT_BLK_TAA_UID_LH_AST_SI_ZOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_taa[] = {
	GOUT_BLK_TAA_UID_PPMU_TAA_IPCLKPORT_ACLK,
	GOUT_BLK_TAA_UID_PPMU_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf1_csistaa[] = {
	GOUT_BLK_TAA_UID_LH_AST_MI_OTF1_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_sotf0_taacsis[] = {
	GOUT_BLK_TAA_UID_LH_AST_SI_SOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_sotf1_taacsis[] = {
	GOUT_BLK_TAA_UID_LH_AST_SI_SOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_taa[] = {
	GOUT_BLK_TAA_UID_AD_APB_TAA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_taa[] = {
	GOUT_BLK_TAA_UID_SYSMMU_TAA_IPCLKPORT_CLK_S1,
	GOUT_BLK_TAA_UID_SYSMMU_TAA_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d_taa[] = {
	GOUT_BLK_TAA_UID_XIU_D_TAA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf2_csistaa[] = {
	GOUT_BLK_TAA_UID_LH_AST_MI_OTF2_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_zotf2_taacsis[] = {
	GOUT_BLK_TAA_UID_LH_AST_SI_ZOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_sotf2_taacsis[] = {
	GOUT_BLK_TAA_UID_LH_AST_SI_SOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite0_taa[] = {
	GOUT_BLK_TAA_UID_VGEN_LITE0_TAA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite1_taa[] = {
	GOUT_BLK_TAA_UID_VGEN_LITE1_TAA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_tnr_cmu_tnr[] = {
	CLK_BLK_TNR_UID_TNR_CMU_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_tnr[] = {
	GOUT_BLK_TNR_UID_SLH_AXI_MI_P_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apb_async_tnr_0[] = {
	GOUT_BLK_TNR_UID_APB_ASYNC_TNR_0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_tnr[] = {
	GOUT_BLK_TNR_UID_LH_AXI_SI_D0_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_tnr[] = {
	GOUT_BLK_TNR_UID_LH_AXI_SI_D1_TNR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_tnr[] = {
	GOUT_BLK_TNR_UID_SYSREG_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_tnr[] = {
	GOUT_BLK_TNR_UID_D_TZPC_TNR_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_tnr[] = {
	GOUT_BLK_TNR_UID_XIU_D1_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_tnr[] = {
	GOUT_BLK_TNR_UID_PPMU_D0_TNR_IPCLKPORT_PCLK,
	GOUT_BLK_TNR_UID_PPMU_D0_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_tnr[] = {
	GOUT_BLK_TNR_UID_PPMU_D1_TNR_IPCLKPORT_PCLK,
	GOUT_BLK_TNR_UID_PPMU_D1_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_tnr[] = {
	GOUT_BLK_TNR_UID_SYSMMU_D0_TNR_IPCLKPORT_CLK_S1,
	GOUT_BLK_TNR_UID_SYSMMU_D0_TNR_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_tnr[] = {
	GOUT_BLK_TNR_UID_SYSMMU_D1_TNR_IPCLKPORT_CLK_S1,
	GOUT_BLK_TNR_UID_SYSMMU_D1_TNR_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_tnr[] = {
	GOUT_BLK_TNR_UID_TNR_IPCLKPORT_ACLK_MCFP0,
	GOUT_BLK_TNR_UID_TNR_IPCLKPORT_ACLK_MCFP1,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf0_tnrisp[] = {
	GOUT_BLK_TNR_UID_LH_AST_SI_OTF0_TNRISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_tnr[] = {
	GOUT_BLK_TNR_UID_XIU_D0_TNR_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf1_tnrisp[] = {
	GOUT_BLK_TNR_UID_LH_AST_SI_OTF1_TNRISP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d_tnr[] = {
	GOUT_BLK_TNR_UID_VGEN_LITE_D_TNR_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_usb20drd_top[] = {
	GOUT_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_I_USB20DRD_REF_CLK_26,
	GOUT_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_ACLK_PHYCTRL,
	GOUT_BLK_USB_UID_USB20DRD_TOP_IPCLKPORT_BUS_CLK_EARLY,
};
enum clk_id cmucal_vclk_ip_ppmu_usb[] = {
	GOUT_BLK_USB_UID_PPMU_USB_IPCLKPORT_ACLK,
	GOUT_BLK_USB_UID_PPMU_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d_usb[] = {
	GOUT_BLK_USB_UID_SLH_AXI_SI_D_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_usb[] = {
	GOUT_BLK_USB_UID_VGEN_LITE_USB_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_usb[] = {
	GOUT_BLK_USB_UID_D_TZPC_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_usb[] = {
	GOUT_BLK_USB_UID_SLH_AXI_MI_P_USB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_s2mpu_d_usb[] = {
	GOUT_BLK_USB_UID_S2MPU_D_USB_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_usb[] = {
	GOUT_BLK_USB_UID_SYSREG_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usb_cmu_usb[] = {
	CLK_BLK_USB_UID_USB_CMU_USB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_usb[] = {
	GOUT_BLK_USB_UID_XIU_D_USB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d_usbaud[] = {
	GOUT_BLK_USB_UID_SLH_AXI_SI_D_USBAUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_uram[] = {
	GOUT_BLK_USB_UID_URAM_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix_vts[] = {
	GOUT_BLK_VTS_UID_AHB_BUSMATRIX_VTS_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if0[] = {
	GOUT_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vts[] = {
	GOUT_BLK_VTS_UID_SYSREG_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vts_cmu_vts[] = {
	CLK_BLK_VTS_UID_VTS_CMU_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_vts[] = {
	GOUT_BLK_VTS_UID_GPIO_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_vts[] = {
	GOUT_BLK_VTS_UID_WDT_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb0[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_asyncinterrupt_vts[] = {
	GOUT_BLK_VTS_UID_ASYNCINTERRUPT_VTS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ss_vts_glue[] = {
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_ACLK_CPU,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK0,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK1,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK0,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK1,
};
enum clk_id cmucal_vclk_ip_cm4_vts[] = {
	GOUT_BLK_VTS_UID_CM4_VTS_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_abox_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_ABOX_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb2[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_AP_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer_vts[] = {
	GOUT_BLK_VTS_UID_TIMER_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if1[] = {
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_s_vts[] = {
	GOUT_BLK_VTS_UID_SLH_AXI_MI_S_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_m_vts[] = {
	GOUT_BLK_VTS_UID_SLH_AXI_SI_M_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic0[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic2[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_u_dmic_clk_scan_mux[] = {
	CLK_BLK_VTS_UID_U_DMIC_CLK_SCAN_MUX_IPCLKPORT_D0,
};
enum clk_id cmucal_vclk_ip_axi2ahb_vts[] = {
	GOUT_BLK_VTS_UID_AXI2AHB_VTS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ahb2axi_vts[] = {
	GOUT_BLK_VTS_UID_AHB2AXI_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_serial_lif_aud[] = {
	GOUT_BLK_VTS_UID_SERIAL_LIF_AUD_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_AUD_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_AUD_IPCLKPORT_BCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_AUD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud0[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud1[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_DIV2_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_serial_lif[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK,
};
/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_vclk_vdd_mif_lut[] = {
	{4266000, vdd_mif_nm_lut_params},
	{3078000, vdd_mif_ud_lut_params},
	{1410000, vdd_mif_sud_lut_params},
	{710000, vdd_mif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl0_lut[] = {
	{2000000, vdd_cpucl0_sod_lut_params},
	{1700000, vdd_cpucl0_od_lut_params},
	{1500000, vdd_cpucl0_nm_lut_params},
	{1000000, vdd_cpucl0_ud_lut_params},
	{450000, vdd_cpucl0_sud_lut_params},
	{200000, vdd_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_alive_lut[] = {
	{200000, vdd_alive_uud_lut_params},
	{100000, vdd_alive_nm_lut_params},
};
struct vclk_lut cmucal_vclk_vddi_lut[] = {
	{1599000, vddi_od_lut_params},
	{1599000, vddi_sud_lut_params},
	{1599000, vddi_ud_lut_params},
	{1599000, vddi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl1_lut[] = {
	{2600000, vdd_cpucl1_sod_lut_params},
	{2000000, vdd_cpucl1_od_lut_params},
	{1700000, vdd_cpucl1_nm_lut_params},
	{1100000, vdd_cpucl1_ud_lut_params},
	{500000, vdd_cpucl1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_aud_lut[] = {
	{1500000, vdd_aud_od_lut_params},
	{1200000, vdd_aud_nm_lut_params},
	{1200000, vdd_aud_sud_lut_params},
	{1200000, vdd_aud_ud_lut_params},
	{1200000, vdd_aud_uud_lut_params},
};

/* SPECIAL VCLK -> LUT List */
struct vclk_lut cmucal_vclk_mux_clk_alive_i3c_pmic_lut[] = {
	{215250, mux_clk_alive_i3c_pmic_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_alive_bus_lut[] = {
	{399750, clkcmu_alive_bus_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_pcmc_lut[] = {
	{750000, div_clk_aud_pcmc_od_lut_params},
	{600000, div_clk_aud_pcmc_nm_lut_params},
	{399750, div_clk_aud_pcmc_ud_lut_params},
	{50000, div_clk_aud_pcmc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_busc_cmuref_lut[] = {
	{199875, mux_busc_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_lut[] = {
	{399750, mux_clkcmu_cmu_boost_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_chub_timer_lut[] = {
	{30000, mux_clk_chub_timer_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cmu_cmuref_lut[] = {
	{26000, mux_cmu_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_core_cmuref_lut[] = {
	{199875, mux_core_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl0_cmuref_lut[] = {
	{199875, mux_cpucl0_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_dsu_cmuref_lut[] = {
	{26000, mux_dsu_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif_cmuref_lut[] = {
	{199875, mux_mif_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_usb_usb20drd_lut[] = {
	{26000, mux_clk_usb_usb20drd_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_usb_usb20drd_lut[] = {
	{26000, clkcmu_usb_usb20drd_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_dpu_dsim_lut[] = {
	{79950, clkcmu_dpu_dsim_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_peri_mmc_card_lut[] = {
	{808000, mux_clkcmu_peri_mmc_card_ud_lut_params},
	{806000, mux_clkcmu_peri_mmc_card_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_alive_dbgcore_uart_lut[] = {
	{215250, div_clk_alive_dbgcore_uart_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_alive_usi0_lut[] = {
	{430500, div_clk_alive_usi0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_alive_i2c_lut[] = {
	{215250, div_clk_alive_i2c_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_aud_cpu_lut[] = {
	{799500, mux_clkcmu_aud_cpu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_mclk_lut[] = {
	{50000, div_clk_aud_mclk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi0_lut[] = {
	{430500, div_clk_chub_usi0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_chub_peri_lut[] = {
	{430500, clkcmu_chub_peri_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi1_lut[] = {
	{430500, div_clk_chub_usi1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi2_lut[] = {
	{430500, div_clk_chub_usi2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi3_lut[] = {
	{430500, div_clk_chub_usi3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi0_lut[] = {
	{430500, div_clk_cmgp_usi0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cmgp_peri_lut[] = {
	{430500, clkcmu_cmgp_peri_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi4_lut[] = {
	{430500, div_clk_cmgp_usi4_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_i3c_lut[] = {
	{215250, div_clk_cmgp_i3c_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi1_lut[] = {
	{430500, div_clk_cmgp_usi1_nm_lut_params},
	{60000, div_clk_cmgp_usi1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi2_lut[] = {
	{430500, div_clk_cmgp_usi2_nm_lut_params},
	{60000, div_clk_cmgp_usi2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi3_lut[] = {
	{430500, div_clk_cmgp_usi3_nm_lut_params},
	{60000, div_clk_cmgp_usi3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk0_lut[] = {
	{99938, clkcmu_cis_clk0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk1_lut[] = {
	{99938, clkcmu_cis_clk1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk2_lut[] = {
	{99938, clkcmu_cis_clk2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk3_lut[] = {
	{99938, clkcmu_cis_clk3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk4_lut[] = {
	{99938, clkcmu_cis_clk4_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk5_lut[] = {
	{99938, clkcmu_cis_clk5_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl0_shortstop_lut[] = {
	{1000000, div_clk_cpucl0_shortstop_sod_lut_params},
	{850000, div_clk_cpucl0_shortstop_od_lut_params},
	{750000, div_clk_cpucl0_shortstop_nm_lut_params},
	{500000, div_clk_cpucl0_shortstop_ud_lut_params},
	{225000, div_clk_cpucl0_shortstop_sud_lut_params},
	{100000, div_clk_cpucl0_shortstop_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_shortstop_lut[] = {
	{1300000, div_clk_cpucl1_shortstop_sod_lut_params},
	{1000000, div_clk_cpucl1_shortstop_od_lut_params},
	{850000, div_clk_cpucl1_shortstop_nm_lut_params},
	{550000, div_clk_cpucl1_shortstop_ud_lut_params},
	{250000, div_clk_cpucl1_shortstop_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_htu_lut[] = {
	{1300000, div_clk_cpucl1_htu_sod_lut_params},
	{1000000, div_clk_cpucl1_htu_od_lut_params},
	{850000, div_clk_cpucl1_htu_nm_lut_params},
	{550000, div_clk_cpucl1_htu_ud_lut_params},
	{250000, div_clk_cpucl1_htu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_dsu_shortstop_lut[] = {
	{900000, div_clk_dsu_shortstop_sod_lut_params},
	{750000, div_clk_dsu_shortstop_od_lut_params},
	{600000, div_clk_dsu_shortstop_nm_lut_params},
	{400000, div_clk_dsu_shortstop_ud_lut_params},
	{200000, div_clk_dsu_shortstop_sud_lut_params},
	{75000, div_clk_dsu_shortstop_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cluster0_atclk_lut[] = {
	{450000, div_clk_cluster0_atclk_sod_lut_params},
	{375000, div_clk_cluster0_atclk_od_lut_params},
	{300000, div_clk_cluster0_atclk_nm_lut_params},
	{200000, div_clk_cluster0_atclk_ud_lut_params},
	{100000, div_clk_cluster0_atclk_sud_lut_params},
	{37500, div_clk_cluster0_atclk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi00_usi_lut[] = {
	{399750, div_clk_peri_usi00_usi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_peri_ip_lut[] = {
	{399750, mux_clkcmu_peri_ip_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi01_usi_lut[] = {
	{399750, div_clk_peri_usi01_usi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi02_usi_lut[] = {
	{399750, div_clk_peri_usi02_usi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi03_usi_lut[] = {
	{399750, div_clk_peri_usi03_usi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi04_usi_lut[] = {
	{399750, div_clk_peri_usi04_usi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi05_usi_lut[] = {
	{399750, div_clk_peri_usi05_usi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_uart_dbg_lut[] = {
	{199875, div_clk_peri_uart_dbg_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peri_usi06_usi_lut[] = {
	{399750, div_clk_peri_usi06_usi_uud_lut_params},
};

struct vclk_lut cmucal_vclk_div_clk_peri_usixx_usi_lut[] = {
	{400000, div_clk_peri_400_lut_params},
	{200000, div_clk_peri_200_lut_params},
	{133000, div_clk_peri_133_lut_params},
	{100000, div_clk_peri_100_lut_params},
	{80000, div_clk_peri_80_lut_params},
	{66000, div_clk_peri_66_lut_params},
	{57000, div_clk_peri_57_lut_params},
	{50000, div_clk_peri_50_lut_params},
	{44000, div_clk_peri_44_lut_params},
	{40000, div_clk_peri_40_lut_params},
	{36000, div_clk_peri_36_lut_params},
	{33000, div_clk_peri_33_lut_params},
	{30000, div_clk_peri_30_lut_params},
	{28000, div_clk_peri_28_lut_params},
	{26000, div_clk_peri_26_lut_params},
	{25000, div_clk_peri_25_lut_params},
	{13000, div_clk_peri_13_lut_params},
	{8600, div_clk_peri_8_lut_params},
	{6500, div_clk_peri_6_lut_params},
	{5200, div_clk_peri_5_lut_params},
	{4300, div_clk_peri_4_lut_params},
	{3700, div_clk_peri_3_lut_params},
};
struct vclk_lut cmucal_vclk_div_vts_serial_lif_core_lut[] = {
	{430500, div_vts_serial_lif_core_nm_lut_params},
	{430500, div_vts_serial_lif_core_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_chubvts_bus_lut[] = {
	{430500, clkcmu_chubvts_bus_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_ap2gnss_lut[] = {
	{430500, mux_clkcmu_ap2gnss_uud_lut_params},
};

/* COMMON VCLK -> LUT List */
struct vclk_lut cmucal_vclk_blk_cmu_lut[] = {
	{1100000, blk_cmu_nm_lut_params},
	{936000, blk_cmu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_s2d_lut[] = {
	{6500, blk_s2d_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_alive_lut[] = {
	{430500, blk_alive_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_aud_lut[] = {
	{750000, blk_aud_od_lut_params},
	{600000, blk_aud_nm_lut_params},
	{399750, blk_aud_ud_lut_params},
	{177667, blk_aud_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_chub_lut[] = {
	{430500, blk_chub_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_chubvts_lut[] = {
	{430500, blk_chubvts_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cmgp_lut[] = {
	{430500, blk_cmgp_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_core_lut[] = {
	{333250, blk_core_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_lut[] = {
	{2000000, blk_cpucl0_sod_lut_params},
	{1700000, blk_cpucl0_od_lut_params},
	{1500000, blk_cpucl0_nm_lut_params},
	{1000000, blk_cpucl0_ud_lut_params},
	{450000, blk_cpucl0_sud_lut_params},
	{200000, blk_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl1_lut[] = {
	{2600000, blk_cpucl1_sod_lut_params},
	{2000000, blk_cpucl1_od_lut_params},
	{1700000, blk_cpucl1_nm_lut_params},
	{1100000, blk_cpucl1_ud_lut_params},
	{500000, blk_cpucl1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dsu_lut[] = {
	{450000, blk_dsu_sod_lut_params},
	{375000, blk_dsu_od_lut_params},
	{300000, blk_dsu_nm_lut_params},
	{200000, blk_dsu_ud_lut_params},
	{100000, blk_dsu_sud_lut_params},
	{37500, blk_dsu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_usb_lut[] = {
	{266500, blk_usb_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vts_lut[] = {
	{430500, blk_vts_nm_lut_params},
	{430500, blk_vts_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_busc_lut[] = {
	{333250, blk_busc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_glb_lut[] = {
	{199875, blk_cpucl0_glb_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_csis_lut[] = {
	{333250, blk_csis_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dpu_lut[] = {
	{333250, blk_dpu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_g3d_lut[] = {
	{275000, blk_g3d_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_isp_lut[] = {
	{333250, blk_isp_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_m2m_lut[] = {
	{399750, blk_m2m_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mcsc_lut[] = {
	{333250, blk_mcsc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mfc_lut[] = {
	{333250, blk_mfc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npu0_lut[] = {
	{234000, blk_npu0_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npus_lut[] = {
	{199875, blk_npus_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peri_lut[] = {
	{199875, blk_peri_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_taa_lut[] = {
	{333250, blk_taa_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_tnr_lut[] = {
	{333250, blk_tnr_uud_lut_params},
};

/* Switch VCLK -> LUT Parameter List */
struct switch_lut mux_clk_aud_pcmc_lut[] = {
};
/*================================ SWPLL List =================================*/
struct vclk_switch switch_vdd_aud[] = {
	{MUX_CLK_AUD_PCMC, EMPTY_CAL_ID, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CP_PCMC_CLK_USER, mux_clk_aud_pcmc_lut, 0},
};


/*================================ VCLK List =================================*/
unsigned int cmucal_vclk_size = 680;
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
	CMUCAL_VCLK(VCLK_VDD_MIF, cmucal_vclk_vdd_mif_lut, cmucal_vclk_vdd_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_vclk_vdd_cpucl0_lut, cmucal_vclk_vdd_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_ALIVE, cmucal_vclk_vdd_alive_lut, cmucal_vclk_vdd_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDDI, cmucal_vclk_vddi_lut, cmucal_vclk_vddi, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CPUCL1, cmucal_vclk_vdd_cpucl1_lut, cmucal_vclk_vdd_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_AUD, cmucal_vclk_vdd_aud_lut, cmucal_vclk_vdd_aud, NULL, switch_vdd_aud),

/* SPECIAL VCLK*/
	CMUCAL_VCLK(VCLK_MUX_CLK_ALIVE_I3C_PMIC, cmucal_vclk_mux_clk_alive_i3c_pmic_lut, cmucal_vclk_mux_clk_alive_i3c_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_ALIVE_BUS, cmucal_vclk_clkcmu_alive_bus_lut, cmucal_vclk_clkcmu_alive_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_PCMC, cmucal_vclk_div_clk_aud_pcmc_lut, cmucal_vclk_div_clk_aud_pcmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_BUSC_CMUREF, cmucal_vclk_mux_busc_cmuref_lut, cmucal_vclk_mux_busc_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST, cmucal_vclk_mux_clkcmu_cmu_boost_lut, cmucal_vclk_mux_clkcmu_cmu_boost, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_CHUB_TIMER, cmucal_vclk_mux_clk_chub_timer_lut, cmucal_vclk_mux_clk_chub_timer, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CMU_CMUREF, cmucal_vclk_mux_cmu_cmuref_lut, cmucal_vclk_mux_cmu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CORE_CMUREF, cmucal_vclk_mux_core_cmuref_lut, cmucal_vclk_mux_core_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL0_CMUREF, cmucal_vclk_mux_cpucl0_cmuref_lut, cmucal_vclk_mux_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_DSU_CMUREF, cmucal_vclk_mux_dsu_cmuref_lut, cmucal_vclk_mux_dsu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF_CMUREF, cmucal_vclk_mux_mif_cmuref_lut, cmucal_vclk_mux_mif_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_USB_USB20DRD, cmucal_vclk_mux_clk_usb_usb20drd_lut, cmucal_vclk_mux_clk_usb_usb20drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_USB_USB20DRD, cmucal_vclk_clkcmu_usb_usb20drd_lut, cmucal_vclk_clkcmu_usb_usb20drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_DPU_DSIM, cmucal_vclk_clkcmu_dpu_dsim_lut, cmucal_vclk_clkcmu_dpu_dsim, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_PERI_MMC_CARD, cmucal_vclk_mux_clkcmu_peri_mmc_card_lut, cmucal_vclk_mux_clkcmu_peri_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_ALIVE_DBGCORE_UART, cmucal_vclk_div_clk_alive_dbgcore_uart_lut, cmucal_vclk_div_clk_alive_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_ALIVE_USI0, cmucal_vclk_div_clk_alive_usi0_lut, cmucal_vclk_div_clk_alive_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_ALIVE_I2C, cmucal_vclk_div_clk_alive_i2c_lut, cmucal_vclk_div_clk_alive_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_AUD_CPU, cmucal_vclk_mux_clkcmu_aud_cpu_lut, cmucal_vclk_mux_clkcmu_aud_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_MCLK, cmucal_vclk_div_clk_aud_mclk_lut, cmucal_vclk_div_clk_aud_mclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI0, cmucal_vclk_div_clk_chub_usi0_lut, cmucal_vclk_div_clk_chub_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CHUB_PERI, cmucal_vclk_clkcmu_chub_peri_lut, cmucal_vclk_clkcmu_chub_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI1, cmucal_vclk_div_clk_chub_usi1_lut, cmucal_vclk_div_clk_chub_usi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI2, cmucal_vclk_div_clk_chub_usi2_lut, cmucal_vclk_div_clk_chub_usi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI3, cmucal_vclk_div_clk_chub_usi3_lut, cmucal_vclk_div_clk_chub_usi3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI0, cmucal_vclk_div_clk_cmgp_usi0_lut, cmucal_vclk_div_clk_cmgp_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CMGP_PERI, cmucal_vclk_clkcmu_cmgp_peri_lut, cmucal_vclk_clkcmu_cmgp_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI4, cmucal_vclk_div_clk_cmgp_usi4_lut, cmucal_vclk_div_clk_cmgp_usi4, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_I3C, cmucal_vclk_div_clk_cmgp_i3c_lut, cmucal_vclk_div_clk_cmgp_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI1, cmucal_vclk_div_clk_cmgp_usi1_lut, cmucal_vclk_div_clk_cmgp_usi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI2, cmucal_vclk_div_clk_cmgp_usi2_lut, cmucal_vclk_div_clk_cmgp_usi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI3, cmucal_vclk_div_clk_cmgp_usi3_lut, cmucal_vclk_div_clk_cmgp_usi3, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK0, cmucal_vclk_clkcmu_cis_clk0_lut, cmucal_vclk_clkcmu_cis_clk0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK1, cmucal_vclk_clkcmu_cis_clk1_lut, cmucal_vclk_clkcmu_cis_clk1, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK2, cmucal_vclk_clkcmu_cis_clk2_lut, cmucal_vclk_clkcmu_cis_clk2, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK3, cmucal_vclk_clkcmu_cis_clk3_lut, cmucal_vclk_clkcmu_cis_clk3, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK4, cmucal_vclk_clkcmu_cis_clk4_lut, cmucal_vclk_clkcmu_cis_clk4, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK5, cmucal_vclk_clkcmu_cis_clk5_lut, cmucal_vclk_clkcmu_cis_clk5, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL0_SHORTSTOP, cmucal_vclk_div_clk_cpucl0_shortstop_lut, cmucal_vclk_div_clk_cpucl0_shortstop, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_SHORTSTOP, cmucal_vclk_div_clk_cpucl1_shortstop_lut, cmucal_vclk_div_clk_cpucl1_shortstop, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_HTU, cmucal_vclk_div_clk_cpucl1_htu_lut, cmucal_vclk_div_clk_cpucl1_htu, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_DSU_SHORTSTOP, cmucal_vclk_div_clk_dsu_shortstop_lut, cmucal_vclk_div_clk_dsu_shortstop, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CLUSTER0_ATCLK, cmucal_vclk_div_clk_cluster0_atclk_lut, cmucal_vclk_div_clk_cluster0_atclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI00_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_PERI_IP, cmucal_vclk_mux_clkcmu_peri_ip_lut, cmucal_vclk_mux_clkcmu_peri_ip, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI01_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI02_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI03_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI04_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI05_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_UART_DBG, cmucal_vclk_div_clk_peri_uart_dbg_lut, cmucal_vclk_div_clk_peri_uart_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERI_USI06_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmucal_vclk_div_clk_peri_usi06_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_VTS_SERIAL_LIF_CORE, cmucal_vclk_div_vts_serial_lif_core_lut, cmucal_vclk_div_vts_serial_lif_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CHUBVTS_BUS, cmucal_vclk_clkcmu_chubvts_bus_lut, cmucal_vclk_clkcmu_chubvts_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_AP2GNSS, cmucal_vclk_mux_clkcmu_ap2gnss_lut, cmucal_vclk_mux_clkcmu_ap2gnss, NULL, NULL),

/* COMMON VCLK*/
	CMUCAL_VCLK(VCLK_BLK_CMU, cmucal_vclk_blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_S2D, cmucal_vclk_blk_s2d_lut, cmucal_vclk_blk_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ALIVE, cmucal_vclk_blk_alive_lut, cmucal_vclk_blk_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_AUD, cmucal_vclk_blk_aud_lut, cmucal_vclk_blk_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CHUB, cmucal_vclk_blk_chub_lut, cmucal_vclk_blk_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CHUBVTS, cmucal_vclk_blk_chubvts_lut, cmucal_vclk_blk_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMGP, cmucal_vclk_blk_cmgp_lut, cmucal_vclk_blk_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CORE, cmucal_vclk_blk_core_lut, cmucal_vclk_blk_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, cmucal_vclk_blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL1, cmucal_vclk_blk_cpucl1_lut, cmucal_vclk_blk_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DSU, cmucal_vclk_blk_dsu_lut, cmucal_vclk_blk_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_USB, cmucal_vclk_blk_usb_lut, cmucal_vclk_blk_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VTS, cmucal_vclk_blk_vts_lut, cmucal_vclk_blk_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_BUSC, cmucal_vclk_blk_busc_lut, cmucal_vclk_blk_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0_GLB, cmucal_vclk_blk_cpucl0_glb_lut, cmucal_vclk_blk_cpucl0_glb, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CSIS, cmucal_vclk_blk_csis_lut, cmucal_vclk_blk_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DPU, cmucal_vclk_blk_dpu_lut, cmucal_vclk_blk_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G3D, cmucal_vclk_blk_g3d_lut, cmucal_vclk_blk_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ISP, cmucal_vclk_blk_isp_lut, cmucal_vclk_blk_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_M2M, cmucal_vclk_blk_m2m_lut, cmucal_vclk_blk_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MCSC, cmucal_vclk_blk_mcsc_lut, cmucal_vclk_blk_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC, cmucal_vclk_blk_mfc_lut, cmucal_vclk_blk_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPU0, cmucal_vclk_blk_npu0_lut, cmucal_vclk_blk_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPUS, cmucal_vclk_blk_npus_lut, cmucal_vclk_blk_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERI, cmucal_vclk_blk_peri_lut, cmucal_vclk_blk_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_TAA, cmucal_vclk_blk_taa_lut, cmucal_vclk_blk_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_TNR, cmucal_vclk_blk_tnr_lut, cmucal_vclk_blk_tnr, NULL, NULL),

/* GATE VCLK*/
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_APM, NULL, cmucal_vclk_ip_slh_axi_si_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_APM, NULL, cmucal_vclk_ip_slh_axi_mi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_ALIVE, NULL, cmucal_vclk_ip_wdt_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ALIVE, NULL, cmucal_vclk_ip_sysreg_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_AP, NULL, cmucal_vclk_ip_mailbox_apm_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_ALIVE, NULL, cmucal_vclk_ip_apbif_pmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM, NULL, cmucal_vclk_ip_intmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_slh_axi_si_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU_INTR_GEN, NULL, cmucal_vclk_ip_pmu_intr_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_ALIVE, NULL, cmucal_vclk_ip_xiu_dp_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ALIVE_CMU_ALIVE, NULL, cmucal_vclk_ip_alive_cmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION, NULL, cmucal_vclk_ip_grebeintegration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_ALIVE, NULL, cmucal_vclk_ip_apbif_gpio_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_TOP_RTC, NULL, cmucal_vclk_ip_apbif_top_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_DBGCORE, NULL, cmucal_vclk_ip_ss_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_ALIVE, NULL, cmucal_vclk_ip_d_tzpc_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_VTS, NULL, cmucal_vclk_ip_mailbox_apm_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_DBGCORE, NULL, cmucal_vclk_ip_mailbox_ap_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_G_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_si_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_RTC, NULL, cmucal_vclk_ip_apbif_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_C_CMGP, NULL, cmucal_vclk_ip_slh_axi_si_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_ALIVE, NULL, cmucal_vclk_ip_vgen_lite_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DBGCORE_UART, NULL, cmucal_vclk_ip_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ROM_CRC32_HOST, NULL, cmucal_vclk_ip_rom_crc32_host, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_GNSS, NULL, cmucal_vclk_ip_slh_axi_mi_c_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_MODEM, NULL, cmucal_vclk_ip_slh_axi_mi_c_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_CHUBVTS, NULL, cmucal_vclk_ip_slh_axi_mi_c_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_WLBT, NULL, cmucal_vclk_ip_slh_axi_mi_c_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LP_CHUBVTS, NULL, cmucal_vclk_ip_slh_axi_si_lp_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CHUB, NULL, cmucal_vclk_ip_mailbox_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_CHUB, NULL, cmucal_vclk_ip_mailbox_wlbt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_WLBT_ABOX, NULL, cmucal_vclk_ip_mailbox_wlbt_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_WLBT_WL, NULL, cmucal_vclk_ip_mailbox_ap_wlbt_wl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_WLBT, NULL, cmucal_vclk_ip_mailbox_apm_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_WLBT, NULL, cmucal_vclk_ip_mailbox_gnss_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_CHUB, NULL, cmucal_vclk_ip_mailbox_gnss_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_GNSS, NULL, cmucal_vclk_ip_mailbox_ap_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_GNSS, NULL, cmucal_vclk_ip_mailbox_apm_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_GNSS, NULL, cmucal_vclk_ip_mailbox_cp_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_WLBT_WL, NULL, cmucal_vclk_ip_mailbox_cp_wlbt_wl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_CHUB, NULL, cmucal_vclk_ip_mailbox_cp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP_S, NULL, cmucal_vclk_ip_mailbox_ap_cp_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP, NULL, cmucal_vclk_ip_mailbox_ap_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CP, NULL, cmucal_vclk_ip_mailbox_apm_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_VTS_CHUB, NULL, cmucal_vclk_ip_mailbox_vts_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CHUB, NULL, cmucal_vclk_ip_mailbox_ap_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_APM_PMIC, NULL, cmucal_vclk_ip_i3c_apm_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_SYSREG_VGPIO2AP, NULL, cmucal_vclk_ip_apbif_sysreg_vgpio2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_SYSREG_VGPIO2APM, NULL, cmucal_vclk_ip_apbif_sysreg_vgpio2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_SYSREG_VGPIO2PMU, NULL, cmucal_vclk_ip_apbif_sysreg_vgpio2pmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_CHUB_RTC, NULL, cmucal_vclk_ip_apbif_chub_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_WLBT_BT, NULL, cmucal_vclk_ip_mailbox_ap_wlbt_bt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_WLBT_BT, NULL, cmucal_vclk_ip_mailbox_cp_wlbt_bt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HW_SCANDUMP_CLKSTOP_CTRL, NULL, cmucal_vclk_ip_hw_scandump_clkstop_ctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_P_ALIVE, NULL, cmucal_vclk_ip_sweeper_p_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_ALIVE0, NULL, cmucal_vclk_ip_i2c_alive0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_ALIVE0, NULL, cmucal_vclk_ip_usi_alive0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_SHARED_SRAM, NULL, cmucal_vclk_ip_mailbox_shared_sram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AUD_CMU_AUD, NULL, cmucal_vclk_ip_aud_cmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_AUD, NULL, cmucal_vclk_ip_lh_axi_si_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_AUD, NULL, cmucal_vclk_ip_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_AUD, NULL, cmucal_vclk_ip_slh_axi_mi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AUD, NULL, cmucal_vclk_ip_sysmmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD_NS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD_S, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_AUD_S2, NULL, cmucal_vclk_ip_ad_apb_sysmmu_aud_s2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DFTMUX_AUD, NULL, cmucal_vclk_ip_dftmux_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD0, NULL, cmucal_vclk_ip_mailbox_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD1, NULL, cmucal_vclk_ip_mailbox_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_AUD, NULL, cmucal_vclk_ip_d_tzpc_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_USBAUD, NULL, cmucal_vclk_ip_slh_axi_mi_d_usbaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_AUD, NULL, cmucal_vclk_ip_vgen_lite_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGENLITE_AUD, NULL, cmucal_vclk_ip_ad_apb_vgenlite_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_AUD, NULL, cmucal_vclk_ip_sysreg_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSC_CMU_BUSC, NULL, cmucal_vclk_ip_busc_cmu_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA, NULL, cmucal_vclk_ip_ad_apb_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_BUSC, NULL, cmucal_vclk_ip_xiu_p_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_BUSC, NULL, cmucal_vclk_ip_xiu_d_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_PERI, NULL, cmucal_vclk_ip_slh_axi_mi_d_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_USB, NULL, cmucal_vclk_ip_slh_axi_mi_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_MFC, NULL, cmucal_vclk_ip_lh_axi_mi_d_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_APM, NULL, cmucal_vclk_ip_slh_axi_mi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_BUSC, NULL, cmucal_vclk_ip_pdma_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA_BUSC, NULL, cmucal_vclk_ip_spdma_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AXI_D_BUSC, NULL, cmucal_vclk_ip_sysmmu_axi_d_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BUSC, NULL, cmucal_vclk_ip_sysreg_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_BUSC, NULL, cmucal_vclk_ip_trex_d_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_PDMA, NULL, cmucal_vclk_ip_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_BUSC, NULL, cmucal_vclk_ip_slh_axi_mi_p_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BUSC, NULL, cmucal_vclk_ip_d_tzpc_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_CHUBVTS, NULL, cmucal_vclk_ip_lh_axi_mi_d_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_SPDMA, NULL, cmucal_vclk_ip_vgen_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_CHUB, NULL, cmucal_vclk_ip_apbif_gpio_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_CHUB_COMBINE_WAKEUP_SRC, NULL, cmucal_vclk_ip_apbif_chub_combine_wakeup_src, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CM4_CHUB, NULL, cmucal_vclk_ip_cm4_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_CHUB, NULL, cmucal_vclk_ip_pwm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CHUB, NULL, cmucal_vclk_ip_sysreg_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER_CHUB, NULL, cmucal_vclk_ip_timer_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CHUB, NULL, cmucal_vclk_ip_wdt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB1, NULL, cmucal_vclk_ip_i2c_chub1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB0, NULL, cmucal_vclk_ip_usi_chub0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB1, NULL, cmucal_vclk_ip_usi_chub1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB2, NULL, cmucal_vclk_ip_usi_chub2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2AP, NULL, cmucal_vclk_ip_sysreg_combine_chub2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2APM, NULL, cmucal_vclk_ip_sysreg_combine_chub2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2WLBT, NULL, cmucal_vclk_ip_sysreg_combine_chub2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_CHUBEINT, NULL, cmucal_vclk_ip_apbif_gpio_chubeint, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB3, NULL, cmucal_vclk_ip_i2c_chub3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB3, NULL, cmucal_vclk_ip_usi_chub3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX_CHUB, NULL, cmucal_vclk_ip_ahb_busmatrix_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_M_CHUB, NULL, cmucal_vclk_ip_slh_axi_si_m_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_S_CHUB, NULL, cmucal_vclk_ip_slh_axi_mi_s_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_CMU_CHUB, NULL, cmucal_vclk_ip_chub_cmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUBVTS_CMU_CHUBVTS, NULL, cmucal_vclk_ip_chubvts_cmu_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_VTS, NULL, cmucal_vclk_ip_baaw_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CHUBVTS, NULL, cmucal_vclk_ip_d_tzpc_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_CHUBVTS, NULL, cmucal_vclk_ip_lh_axi_si_d_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_M_CHUB, NULL, cmucal_vclk_ip_slh_axi_mi_m_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LP_CHUBVTS, NULL, cmucal_vclk_ip_slh_axi_mi_lp_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_M_VTS, NULL, cmucal_vclk_ip_slh_axi_mi_m_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_C_CHUBVTS, NULL, cmucal_vclk_ip_slh_axi_si_c_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_S_CHUB, NULL, cmucal_vclk_ip_slh_axi_si_s_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_S_VTS, NULL, cmucal_vclk_ip_slh_axi_si_s_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_C_CHUBVTS, NULL, cmucal_vclk_ip_sweeper_c_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CHUBVTS, NULL, cmucal_vclk_ip_sysreg_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_CHUBVTS, NULL, cmucal_vclk_ip_vgen_lite_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CHUBVTS, NULL, cmucal_vclk_ip_xiu_dp_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_LP_CHUBVTS, NULL, cmucal_vclk_ip_bps_lp_chubvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_CHUB, NULL, cmucal_vclk_ip_baaw_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_CMU_CMGP, NULL, cmucal_vclk_ip_cmgp_cmu_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CMGP, NULL, cmucal_vclk_ip_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP0, NULL, cmucal_vclk_ip_i2c_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP, NULL, cmucal_vclk_ip_sysreg_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP0, NULL, cmucal_vclk_ip_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_AP, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CMGP, NULL, cmucal_vclk_ip_d_tzpc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_CMGP, NULL, cmucal_vclk_ip_slh_axi_mi_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2APM, NULL, cmucal_vclk_ip_sysreg_cmgp2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CP, NULL, cmucal_vclk_ip_sysreg_cmgp2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_CMGP, NULL, cmucal_vclk_ip_i3c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2GNSS, NULL, cmucal_vclk_ip_sysreg_cmgp2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2WLBT, NULL, cmucal_vclk_ip_sysreg_cmgp2wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP4, NULL, cmucal_vclk_ip_i2c_cmgp4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP4, NULL, cmucal_vclk_ip_usi_cmgp4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP1, NULL, cmucal_vclk_ip_i2c_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP2, NULL, cmucal_vclk_ip_i2c_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP3, NULL, cmucal_vclk_ip_i2c_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP1, NULL, cmucal_vclk_ip_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP2, NULL, cmucal_vclk_ip_usi_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP3, NULL, cmucal_vclk_ip_usi_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORE_CMU_CORE, NULL, cmucal_vclk_ip_core_cmu_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CORE, NULL, cmucal_vclk_ip_sysreg_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIREX, NULL, cmucal_vclk_ip_sirex, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_CORE, NULL, cmucal_vclk_ip_trex_p_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DIT, NULL, cmucal_vclk_ip_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NRT, NULL, cmucal_vclk_ip_trex_d_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_G3D, NULL, cmucal_vclk_ip_slh_axi_si_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CPUCL0, NULL, cmucal_vclk_ip_slh_axi_si_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CSIS, NULL, cmucal_vclk_ip_slh_axi_si_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_HSI, NULL, cmucal_vclk_ip_slh_axi_mi_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CORE, NULL, cmucal_vclk_ip_trex_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_APM, NULL, cmucal_vclk_ip_slh_axi_si_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CORE, NULL, cmucal_vclk_ip_d_tzpc_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_WLBT, NULL, cmucal_vclk_ip_slh_axi_mi_d_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_WLBT, NULL, cmucal_vclk_ip_slh_axi_si_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D0_MODEM, NULL, cmucal_vclk_ip_slh_axi_mi_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D1_MODEM, NULL, cmucal_vclk_ip_slh_axi_mi_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_GNSS, NULL, cmucal_vclk_ip_slh_axi_mi_d_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_MIF_NRT, NULL, cmucal_vclk_ip_lh_axi_si_d0_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DIT, NULL, cmucal_vclk_ip_ad_apb_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GIC, NULL, cmucal_vclk_ip_ad_axi_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_USB, NULL, cmucal_vclk_ip_slh_axi_si_p_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MODEM, NULL, cmucal_vclk_ip_slh_axi_si_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MIF0, NULL, cmucal_vclk_ip_slh_axi_si_p_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MIF1, NULL, cmucal_vclk_ip_slh_axi_si_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MFC, NULL, cmucal_vclk_ip_slh_axi_si_p_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_GNSS, NULL, cmucal_vclk_ip_slh_axi_si_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC, NULL, cmucal_vclk_ip_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_AUD, NULL, cmucal_vclk_ip_lh_axi_mi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MIF_CP, NULL, cmucal_vclk_ip_lh_axi_si_d1_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MIF_NRT, NULL, cmucal_vclk_ip_lh_axi_si_d1_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_AUD, NULL, cmucal_vclk_ip_slh_axi_si_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DPU, NULL, cmucal_vclk_ip_slh_axi_si_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_HSI, NULL, cmucal_vclk_ip_slh_axi_si_p_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_TAA, NULL, cmucal_vclk_ip_slh_axi_si_p_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_GNSS, NULL, cmucal_vclk_ip_baaw_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_MODEM, NULL, cmucal_vclk_ip_baaw_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_WLBT, NULL, cmucal_vclk_ip_baaw_p_wlbt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFR_APBIF_CMU_TOPC, NULL, cmucal_vclk_ip_sfr_apbif_cmu_topc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_ISP, NULL, cmucal_vclk_ip_slh_axi_si_p_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MCSC, NULL, cmucal_vclk_ip_slh_axi_si_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_TNR, NULL, cmucal_vclk_ip_slh_axi_si_p_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_NPU0, NULL, cmucal_vclk_ip_slh_axi_si_p_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_PERI, NULL, cmucal_vclk_ip_slh_axi_si_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_SSS, NULL, cmucal_vclk_ip_lh_axi_mi_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_ACEL_D_DIT, NULL, cmucal_vclk_ip_sysmmu_acel_d_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_SSS, NULL, cmucal_vclk_ip_lh_axi_si_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_G_CSSYS, NULL, cmucal_vclk_ip_slh_axi_mi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_MIF_CP, NULL, cmucal_vclk_ip_lh_axi_si_d0_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PUF, NULL, cmucal_vclk_ip_puf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PUF, NULL, cmucal_vclk_ip_ad_apb_puf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_NPUS, NULL, cmucal_vclk_ip_slh_axi_si_p_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_MIF_RT, NULL, cmucal_vclk_ip_lh_axi_si_d0_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MIF_RT, NULL, cmucal_vclk_ip_lh_axi_si_d1_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_BUSC, NULL, cmucal_vclk_ip_slh_axi_si_p_busc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MCW, NULL, cmucal_vclk_ip_slh_axi_si_p_mcw, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_G3D, NULL, cmucal_vclk_ip_lh_axi_mi_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_SSS, NULL, cmucal_vclk_ip_baaw_d_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BDU, NULL, cmucal_vclk_ip_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_G_BDU, NULL, cmucal_vclk_ip_xiu_g_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG, NULL, cmucal_vclk_ip_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_DPU, NULL, cmucal_vclk_ip_lh_axi_mi_d0_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_NPUS, NULL, cmucal_vclk_ip_lh_axi_mi_d0_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_NPUS, NULL, cmucal_vclk_ip_lh_axi_mi_d1_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_ACEL_D2_MODEM, NULL, cmucal_vclk_ip_sysmmu_acel_d2_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_BDU, NULL, cmucal_vclk_ip_adm_apb_g_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_DPU, NULL, cmucal_vclk_ip_lh_axi_mi_d1_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_M2M, NULL, cmucal_vclk_ip_slh_axi_si_p_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G_CPU, NULL, cmucal_vclk_ip_lh_ast_mi_g_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CLUSTER0, NULL, cmucal_vclk_ip_slh_axi_mi_p_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_CORE, NULL, cmucal_vclk_ip_vgen_lite_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_LITE_CORE, NULL, cmucal_vclk_ip_ad_apb_vgen_lite_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_M2M, NULL, cmucal_vclk_ip_lh_axi_mi_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HW_APBSEMA_MEC, NULL, cmucal_vclk_ip_hw_apbsema_mec, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL0, NULL, cmucal_vclk_ip_htu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0, NULL, cmucal_vclk_ip_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_GLB_CMU_CPUCL0_GLB, NULL, cmucal_vclk_ip_cpucl0_glb_cmu_cpucl0_glb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_P_CSSYS_0, NULL, cmucal_vclk_ip_apb_async_p_cssys_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_CPUCL0, NULL, cmucal_vclk_ip_xiu_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_CPUCL0, NULL, cmucal_vclk_ip_bps_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS, NULL, cmucal_vclk_ip_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL0, NULL, cmucal_vclk_ip_d_tzpc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_G_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_mi_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_G_INT_CSSYS, NULL, cmucal_vclk_ip_slh_axi_mi_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_mi_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CPUCL0, NULL, cmucal_vclk_ip_slh_axi_mi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_G_CSSYS, NULL, cmucal_vclk_ip_slh_axi_si_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_G_INT_CSSYS, NULL, cmucal_vclk_ip_slh_axi_si_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_si_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CSSYS, NULL, cmucal_vclk_ip_xiu_dp_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CLUSTER0, NULL, cmucal_vclk_ip_adm_apb_g_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1_CMU_CPUCL1, NULL, cmucal_vclk_ip_cpucl1_cmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1, NULL, cmucal_vclk_ip_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL1, NULL, cmucal_vclk_ip_htu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS_CMU_CSIS, NULL, cmucal_vclk_ip_csis_cmu_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CSIS, NULL, cmucal_vclk_ip_d_tzpc_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS_PDP, NULL, cmucal_vclk_ip_csis_pdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CSIS_D0, NULL, cmucal_vclk_ip_ppmu_csis_d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CSIS_D1, NULL, cmucal_vclk_ip_ppmu_csis_d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_CSIS, NULL, cmucal_vclk_ip_sysmmu_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_CSIS, NULL, cmucal_vclk_ip_sysmmu_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CSIS, NULL, cmucal_vclk_ip_sysreg_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CSIS, NULL, cmucal_vclk_ip_slh_axi_mi_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF0_CSISTAA, NULL, cmucal_vclk_ip_lh_ast_si_otf0_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_ZOTF0_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_mi_zotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_ZOTF1_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_mi_zotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS0, NULL, cmucal_vclk_ip_ad_apb_csis0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_CSIS, NULL, cmucal_vclk_ip_xiu_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_CSIS, NULL, cmucal_vclk_ip_xiu_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF1_CSISTAA, NULL, cmucal_vclk_ip_lh_ast_si_otf1_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_SOTF0_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_mi_sotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_SOTF1_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_mi_sotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CSIS_D2, NULL, cmucal_vclk_ip_ppmu_csis_d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA0, NULL, cmucal_vclk_ip_qe_csis_dma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA1, NULL, cmucal_vclk_ip_qe_csis_dma1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL0, NULL, cmucal_vclk_ip_qe_zsl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP0, NULL, cmucal_vclk_ip_qe_strp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT_IMG0, NULL, cmucal_vclk_ip_qe_pdp_stat_img0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIPI_DCPHY_LINK_WRAP, NULL, cmucal_vclk_ip_mipi_dcphy_link_wrap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D2_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL1, NULL, cmucal_vclk_ip_qe_zsl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP1, NULL, cmucal_vclk_ip_qe_strp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT_IMG1, NULL, cmucal_vclk_ip_qe_pdp_stat_img1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA2, NULL, cmucal_vclk_ip_qe_csis_dma2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA3, NULL, cmucal_vclk_ip_qe_csis_dma3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_AF0, NULL, cmucal_vclk_ip_qe_pdp_af0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_AF1, NULL, cmucal_vclk_ip_qe_pdp_af1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D2_CSIS, NULL, cmucal_vclk_ip_xiu_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D3_CSIS, NULL, cmucal_vclk_ip_xiu_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_CSIS, NULL, cmucal_vclk_ip_sysmmu_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D4_CSIS, NULL, cmucal_vclk_ip_xiu_d4_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CSIS_D3, NULL, cmucal_vclk_ip_ppmu_csis_d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D3_CSIS, NULL, cmucal_vclk_ip_sysmmu_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D3_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_AF2, NULL, cmucal_vclk_ip_qe_pdp_af2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP2, NULL, cmucal_vclk_ip_qe_strp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL2, NULL, cmucal_vclk_ip_qe_zsl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT_IMG2, NULL, cmucal_vclk_ip_qe_pdp_stat_img2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF2_CSISTAA, NULL, cmucal_vclk_ip_lh_ast_si_otf2_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_ZOTF2_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_mi_zotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_SOTF2_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_mi_sotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE0_CSIS, NULL, cmucal_vclk_ip_vgen_lite0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE1_CSIS, NULL, cmucal_vclk_ip_vgen_lite1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE2_CSIS, NULL, cmucal_vclk_ip_vgen_lite2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU_CMU_DPU, NULL, cmucal_vclk_ip_dpu_cmu_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPU, NULL, cmucal_vclk_ip_sysreg_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AXI_D0_DPU, NULL, cmucal_vclk_ip_sysmmu_axi_d0_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_DPU, NULL, cmucal_vclk_ip_slh_axi_mi_p_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_DPU, NULL, cmucal_vclk_ip_ppmu_d0_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_DPU, NULL, cmucal_vclk_ip_lh_axi_si_d0_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPU, NULL, cmucal_vclk_ip_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPU, NULL, cmucal_vclk_ip_d_tzpc_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DECON0, NULL, cmucal_vclk_ip_ad_apb_decon0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_DPU, NULL, cmucal_vclk_ip_lh_axi_si_d1_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_DPU, NULL, cmucal_vclk_ip_ppmu_d1_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_AXI_D1_DPU, NULL, cmucal_vclk_ip_sysmmu_axi_d1_dpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CLUSTER0, NULL, cmucal_vclk_ip_slh_axi_si_p_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_CPUCL0, NULL, cmucal_vclk_ip_xiu_d_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSU_CMU_DSU, NULL, cmucal_vclk_ip_dsu_cmu_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_MIF_CPU, NULL, cmucal_vclk_ip_lh_axi_si_d0_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MIF_CPU, NULL, cmucal_vclk_ip_lh_axi_si_d1_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRUN_CLUSTER0_0, NULL, cmucal_vclk_ip_ppc_instrrun_cluster0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRUN_CLUSTER0_1, NULL, cmucal_vclk_ip_ppc_instrrun_cluster0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRET_CLUSTER0_0, NULL, cmucal_vclk_ip_ppc_instrret_cluster0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRET_CLUSTER0_1, NULL, cmucal_vclk_ip_ppc_instrret_cluster0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLUSTER0, NULL, cmucal_vclk_ip_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_DSU, NULL, cmucal_vclk_ip_htu_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL0, NULL, cmucal_vclk_ip_ppmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL1, NULL, cmucal_vclk_ip_ppmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_G_CPU, NULL, cmucal_vclk_ip_lh_ast_si_g_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_G3D, NULL, cmucal_vclk_ip_slh_axi_mi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPU, NULL, cmucal_vclk_ip_gpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G3D, NULL, cmucal_vclk_ip_d_tzpc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_G3D, NULL, cmucal_vclk_ip_lh_axi_si_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_G3D, NULL, cmucal_vclk_ip_htu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_G3D, NULL, cmucal_vclk_ip_ppmu_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_SYSMMU_D_G3D, NULL, cmucal_vclk_ip_as_apb_sysmmu_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_G3D, NULL, cmucal_vclk_ip_sysmmu_d_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_G3D, NULL, cmucal_vclk_ip_gray2bin_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_G3D, NULL, cmucal_vclk_ip_xiu_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_VGENLITE_G3D, NULL, cmucal_vclk_ip_as_apb_vgenlite_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_G3D, NULL, cmucal_vclk_ip_vgen_lite_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GNSS_CMU_GNSS, NULL, cmucal_vclk_ip_gnss_cmu_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI, NULL, cmucal_vclk_ip_vgen_lite_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI_CMU_HSI, NULL, cmucal_vclk_ip_hsi_cmu_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI, NULL, cmucal_vclk_ip_sysreg_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI, NULL, cmucal_vclk_ip_gpio_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_HSI, NULL, cmucal_vclk_ip_slh_axi_si_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_HSI, NULL, cmucal_vclk_ip_slh_axi_mi_p_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI, NULL, cmucal_vclk_ip_ppmu_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI, NULL, cmucal_vclk_ip_d_tzpc_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFS_EMBD, NULL, cmucal_vclk_ip_ufs_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2MPU_D_HSI, NULL, cmucal_vclk_ip_s2mpu_d_hsi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI_UFS, NULL, cmucal_vclk_ip_gpio_hsi_ufs, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ISP, NULL, cmucal_vclk_ip_sysreg_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ISP_CMU_ISP, NULL, cmucal_vclk_ip_isp_cmu_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_ISP, NULL, cmucal_vclk_ip_d_tzpc_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ISP, NULL, cmucal_vclk_ip_ppmu_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_ISP, NULL, cmucal_vclk_ip_lh_axi_si_d_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_ITP, NULL, cmucal_vclk_ip_ad_apb_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITP_DNS, NULL, cmucal_vclk_ip_itp_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_ISP, NULL, cmucal_vclk_ip_sysmmu_d_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF_TAAISP, NULL, cmucal_vclk_ip_lh_ast_mi_otf_taaisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF_ISPMCSC, NULL, cmucal_vclk_ip_lh_ast_si_otf_ispmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF0_TNRISP, NULL, cmucal_vclk_ip_lh_ast_mi_otf0_tnrisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF1_TNRISP, NULL, cmucal_vclk_ip_lh_ast_mi_otf1_tnrisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_ISP, NULL, cmucal_vclk_ip_xiu_d_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_LITE_ISP, NULL, cmucal_vclk_ip_ad_apb_vgen_lite_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_ISP, NULL, cmucal_vclk_ip_vgen_lite_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_ISP, NULL, cmucal_vclk_ip_slh_axi_mi_p_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M, NULL, cmucal_vclk_ip_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M_CMU_M2M, NULL, cmucal_vclk_ip_m2m_cmu_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_M2M, NULL, cmucal_vclk_ip_sysreg_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_M2M, NULL, cmucal_vclk_ip_slh_axi_mi_p_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_M2M, NULL, cmucal_vclk_ip_sysmmu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_M2M, NULL, cmucal_vclk_ip_ppmu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_M2M, NULL, cmucal_vclk_ip_xiu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_JPEG0, NULL, cmucal_vclk_ip_as_apb_jpeg0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG0, NULL, cmucal_vclk_ip_jpeg0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_M2M, NULL, cmucal_vclk_ip_d_tzpc_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_M2M, NULL, cmucal_vclk_ip_lh_axi_si_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_VGEN_LITE_M2M, NULL, cmucal_vclk_ip_as_apb_vgen_lite_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_M2M, NULL, cmucal_vclk_ip_vgen_lite_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC_CMU_MCSC, NULL, cmucal_vclk_ip_mcsc_cmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MCSC, NULL, cmucal_vclk_ip_slh_axi_mi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCSC, NULL, cmucal_vclk_ip_sysreg_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MCSC, NULL, cmucal_vclk_ip_ppmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCSC, NULL, cmucal_vclk_ip_d_tzpc_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_GDC, NULL, cmucal_vclk_ip_ad_apb_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_GDC, NULL, cmucal_vclk_ip_ppmu_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_MCSC, NULL, cmucal_vclk_ip_sysmmu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GDC, NULL, cmucal_vclk_ip_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC, NULL, cmucal_vclk_ip_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_MCSC, NULL, cmucal_vclk_ip_ad_axi_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_GDC, NULL, cmucal_vclk_ip_ad_axi_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CAM, NULL, cmucal_vclk_ip_trex_d_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_TNR, NULL, cmucal_vclk_ip_lh_axi_mi_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_TNR, NULL, cmucal_vclk_ip_lh_axi_mi_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D2_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D3_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_ISP, NULL, cmucal_vclk_ip_lh_axi_mi_d_isp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_TAA, NULL, cmucal_vclk_ip_lh_axi_mi_d_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF_ISPMCSC, NULL, cmucal_vclk_ip_lh_ast_mi_otf_ispmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCSC, NULL, cmucal_vclk_ip_ad_apb_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_MCSC, NULL, cmucal_vclk_ip_sysmmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MCSC, NULL, cmucal_vclk_ip_xiu_d_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ORBMCH, NULL, cmucal_vclk_ip_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_D0_MCSC_NS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_d0_mcsc_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MCSC, NULL, cmucal_vclk_ip_vgen_lite_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_GDC, NULL, cmucal_vclk_ip_vgen_lite_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC_CMU_MFC, NULL, cmucal_vclk_ip_mfc_cmu_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC, NULL, cmucal_vclk_ip_as_apb_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC, NULL, cmucal_vclk_ip_sysreg_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_MFC, NULL, cmucal_vclk_ip_lh_axi_si_d_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MFC, NULL, cmucal_vclk_ip_slh_axi_mi_p_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC, NULL, cmucal_vclk_ip_sysmmu_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC, NULL, cmucal_vclk_ip_ppmu_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC, NULL, cmucal_vclk_ip_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFC, NULL, cmucal_vclk_ip_d_tzpc_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MFC, NULL, cmucal_vclk_ip_vgen_lite_mfc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DDRPHY, NULL, cmucal_vclk_ip_sfrapb_bridge_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MIF, NULL, cmucal_vclk_ip_slh_axi_mi_p_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY, NULL, cmucal_vclk_ip_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF, NULL, cmucal_vclk_ip_d_tzpc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_DMC_CPU, NULL, cmucal_vclk_ip_qe_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DMC_CPU, NULL, cmucal_vclk_ip_ppmu_dmc_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_PF, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_pf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_SECURE, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_secure, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SFRAPB_BRIDGE_DMC_PPMPU, NULL, cmucal_vclk_ip_sfrapb_bridge_dmc_ppmpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_MIF_CPU, NULL, cmucal_vclk_ip_lh_axi_mi_d_mif_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_MIF_RT, NULL, cmucal_vclk_ip_lh_axi_mi_d_mif_rt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_MIF_NRT, NULL, cmucal_vclk_ip_lh_axi_mi_d_mif_nrt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_MIF_CP, NULL, cmucal_vclk_ip_lh_axi_mi_d_mif_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MODEM_CMU_MODEM, NULL, cmucal_vclk_ip_modem_cmu_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU0_CMU_NPU0, NULL, cmucal_vclk_ip_npu0_cmu_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPU0, NULL, cmucal_vclk_ip_d_tzpc_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPU0, NULL, cmucal_vclk_ip_sysreg_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_NPU0, NULL, cmucal_vclk_ip_slh_axi_mi_p_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUCORE, NULL, cmucal_vclk_ip_ip_npucore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_CTRL_NPU0, NULL, cmucal_vclk_ip_lh_axi_mi_d_ctrl_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_NPU0, NULL, cmucal_vclk_ip_lh_axi_mi_d0_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_RQ_NPU0, NULL, cmucal_vclk_ip_lh_axi_si_d_rq_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_CMDQ_NPU0, NULL, cmucal_vclk_ip_lh_axi_si_d_cmdq_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_NPU0, NULL, cmucal_vclk_ip_lh_axi_mi_d1_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_NPUS, NULL, cmucal_vclk_ip_vgen_lite_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_RQ_NPU0, NULL, cmucal_vclk_ip_lh_axi_mi_d_rq_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_CTRL_NPU0, NULL, cmucal_vclk_ip_lh_axi_si_d_ctrl_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_NPU0, NULL, cmucal_vclk_ip_lh_axi_si_d0_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_NPUS, NULL, cmucal_vclk_ip_lh_axi_si_d0_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_NPUS, NULL, cmucal_vclk_ip_lh_axi_si_d1_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_NPUS, NULL, cmucal_vclk_ip_sysmmu_d0_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUS, NULL, cmucal_vclk_ip_ip_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_NPUS_0, NULL, cmucal_vclk_ip_ppmu_npus_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_NPUS, NULL, cmucal_vclk_ip_htu_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_CMDQ_NPU0, NULL, cmucal_vclk_ip_lh_axi_mi_d_cmdq_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_NPUS, NULL, cmucal_vclk_ip_sysmmu_d1_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_NPU0, NULL, cmucal_vclk_ip_lh_axi_si_d1_npu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_NPUS_1, NULL, cmucal_vclk_ip_ppmu_npus_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DS_256_128_0, NULL, cmucal_vclk_ip_ds_256_128_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DS_256_128_1, NULL, cmucal_vclk_ip_ds_256_128_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPUS, NULL, cmucal_vclk_ip_d_tzpc_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_INT_NPUS, NULL, cmucal_vclk_ip_slh_axi_mi_p_int_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_INT_NPUS, NULL, cmucal_vclk_ip_slh_axi_si_p_int_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_D0_NPUS_NS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_d0_npus_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPUS, NULL, cmucal_vclk_ip_sysreg_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_NPUS, NULL, cmucal_vclk_ip_slh_axi_mi_p_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_NPUS, NULL, cmucal_vclk_ip_adm_dap_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPUS_CMU_NPUS, NULL, cmucal_vclk_ip_npus_cmu_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERI, NULL, cmucal_vclk_ip_gpio_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERI, NULL, cmucal_vclk_ip_sysreg_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_CMU_PERI, NULL, cmucal_vclk_ip_peri_cmu_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_PERI, NULL, cmucal_vclk_ip_slh_axi_mi_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERI, NULL, cmucal_vclk_ip_d_tzpc_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_PERI, NULL, cmucal_vclk_ip_xiu_p_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT0, NULL, cmucal_vclk_ip_wdt0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT1, NULL, cmucal_vclk_ip_wdt1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU, NULL, cmucal_vclk_ip_tmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM, NULL, cmucal_vclk_ip_pwm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UART_DBG, NULL, cmucal_vclk_ip_uart_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_USI, NULL, cmucal_vclk_ip_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_I2C, NULL, cmucal_vclk_ip_usi00_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI01_USI, NULL, cmucal_vclk_ip_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI01_I2C, NULL, cmucal_vclk_ip_usi01_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI02_USI, NULL, cmucal_vclk_ip_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI02_I2C, NULL, cmucal_vclk_ip_usi02_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI03_USI, NULL, cmucal_vclk_ip_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI03_I2C, NULL, cmucal_vclk_ip_usi03_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI04_USI, NULL, cmucal_vclk_ip_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI04_I2C, NULL, cmucal_vclk_ip_usi04_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI05_USI, NULL, cmucal_vclk_ip_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI05_I2C, NULL, cmucal_vclk_ip_usi05_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_PERI, NULL, cmucal_vclk_ip_vgen_lite_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2MPU_D_PERI, NULL, cmucal_vclk_ip_s2mpu_d_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_PERI, NULL, cmucal_vclk_ip_ppmu_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_PERI, NULL, cmucal_vclk_ip_slh_axi_si_d_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIMMC, NULL, cmucal_vclk_ip_gpio_perimmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI06_USI, NULL, cmucal_vclk_ip_usi06_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI06_I2C, NULL, cmucal_vclk_ip_usi06_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI07_I2C, NULL, cmucal_vclk_ip_usi07_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2D_CMU_S2D, NULL, cmucal_vclk_ip_s2d_cmu_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BIS_S2D, NULL, cmucal_vclk_ip_bis_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_slh_axi_mi_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_TAA, NULL, cmucal_vclk_ip_lh_axi_si_d_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_TAA, NULL, cmucal_vclk_ip_slh_axi_mi_p_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_TAA, NULL, cmucal_vclk_ip_sysreg_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TAA_CMU_TAA, NULL, cmucal_vclk_ip_taa_cmu_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF_TAAISP, NULL, cmucal_vclk_ip_lh_ast_si_otf_taaisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_TAA, NULL, cmucal_vclk_ip_d_tzpc_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF0_CSISTAA, NULL, cmucal_vclk_ip_lh_ast_mi_otf0_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIPU_TAA, NULL, cmucal_vclk_ip_sipu_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_ZOTF0_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_si_zotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_ZOTF1_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_si_zotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_TAA, NULL, cmucal_vclk_ip_ppmu_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF1_CSISTAA, NULL, cmucal_vclk_ip_lh_ast_mi_otf1_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_SOTF0_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_si_sotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_SOTF1_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_si_sotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_TAA, NULL, cmucal_vclk_ip_ad_apb_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_TAA, NULL, cmucal_vclk_ip_sysmmu_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_TAA, NULL, cmucal_vclk_ip_xiu_d_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF2_CSISTAA, NULL, cmucal_vclk_ip_lh_ast_mi_otf2_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_ZOTF2_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_si_zotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_SOTF2_TAACSIS, NULL, cmucal_vclk_ip_lh_ast_si_sotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE0_TAA, NULL, cmucal_vclk_ip_vgen_lite0_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE1_TAA, NULL, cmucal_vclk_ip_vgen_lite1_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TNR_CMU_TNR, NULL, cmucal_vclk_ip_tnr_cmu_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_TNR, NULL, cmucal_vclk_ip_slh_axi_mi_p_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_TNR_0, NULL, cmucal_vclk_ip_apb_async_tnr_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_TNR, NULL, cmucal_vclk_ip_lh_axi_si_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_TNR, NULL, cmucal_vclk_ip_lh_axi_si_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_TNR, NULL, cmucal_vclk_ip_sysreg_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_TNR, NULL, cmucal_vclk_ip_d_tzpc_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_TNR, NULL, cmucal_vclk_ip_xiu_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_TNR, NULL, cmucal_vclk_ip_ppmu_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_TNR, NULL, cmucal_vclk_ip_ppmu_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_TNR, NULL, cmucal_vclk_ip_sysmmu_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_TNR, NULL, cmucal_vclk_ip_sysmmu_d1_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TNR, NULL, cmucal_vclk_ip_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF0_TNRISP, NULL, cmucal_vclk_ip_lh_ast_si_otf0_tnrisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_TNR, NULL, cmucal_vclk_ip_xiu_d0_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF1_TNRISP, NULL, cmucal_vclk_ip_lh_ast_si_otf1_tnrisp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D_TNR, NULL, cmucal_vclk_ip_vgen_lite_d_tnr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB20DRD_TOP, NULL, cmucal_vclk_ip_usb20drd_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_USB, NULL, cmucal_vclk_ip_ppmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_USB, NULL, cmucal_vclk_ip_slh_axi_si_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_USB, NULL, cmucal_vclk_ip_vgen_lite_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_USB, NULL, cmucal_vclk_ip_d_tzpc_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_USB, NULL, cmucal_vclk_ip_slh_axi_mi_p_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2MPU_D_USB, NULL, cmucal_vclk_ip_s2mpu_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_USB, NULL, cmucal_vclk_ip_sysreg_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB_CMU_USB, NULL, cmucal_vclk_ip_usb_cmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_USB, NULL, cmucal_vclk_ip_xiu_d_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_USBAUD, NULL, cmucal_vclk_ip_slh_axi_si_d_usbaud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_URAM, NULL, cmucal_vclk_ip_uram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX_VTS, NULL, cmucal_vclk_ip_ahb_busmatrix_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF0, NULL, cmucal_vclk_ip_dmic_if0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VTS, NULL, cmucal_vclk_ip_sysreg_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VTS_CMU_VTS, NULL, cmucal_vclk_ip_vts_cmu_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_VTS, NULL, cmucal_vclk_ip_gpio_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_VTS, NULL, cmucal_vclk_ip_wdt_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB0, NULL, cmucal_vclk_ip_dmic_ahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCINTERRUPT_VTS, NULL, cmucal_vclk_ip_asyncinterrupt_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_VTS_GLUE, NULL, cmucal_vclk_ip_ss_vts_glue, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CM4_VTS, NULL, cmucal_vclk_ip_cm4_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_ABOX_VTS, NULL, cmucal_vclk_ip_mailbox_abox_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB2, NULL, cmucal_vclk_ip_dmic_ahb2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_VTS, NULL, cmucal_vclk_ip_mailbox_ap_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER_VTS, NULL, cmucal_vclk_ip_timer_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF1, NULL, cmucal_vclk_ip_dmic_if1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_S_VTS, NULL, cmucal_vclk_ip_slh_axi_mi_s_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_M_VTS, NULL, cmucal_vclk_ip_slh_axi_si_m_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC0, NULL, cmucal_vclk_ip_hwacg_sys_dmic0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC2, NULL, cmucal_vclk_ip_hwacg_sys_dmic2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_U_DMIC_CLK_SCAN_MUX, NULL, cmucal_vclk_ip_u_dmic_clk_scan_mux, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI2AHB_VTS, NULL, cmucal_vclk_ip_axi2ahb_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB2AXI_VTS, NULL, cmucal_vclk_ip_ahb2axi_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SERIAL_LIF_AUD, NULL, cmucal_vclk_ip_serial_lif_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD0, NULL, cmucal_vclk_ip_dmic_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD1, NULL, cmucal_vclk_ip_dmic_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_SERIAL_LIF, NULL, cmucal_vclk_ip_hwacg_sys_serial_lif, NULL, NULL),
};
