#include "../../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_vclk_vddi[] = {
	DIV_CLK_DRCP_NOCP,
	DIV_CLK_LME_NOCP,
	DIV_CLK_MCSC_NOCP,
	DIV_CLK_NOCL0_NOCP,
	DIV_CLK_NOCL1A_NOCP,
	DIV_CLK_NOCL1B_NOCP,
	DIV_CLK_PERIC0_I2C,
	DIV_CLK_PERIC1_UART_BT,
	DIV_CLK_PERIC1_I2C,
	DIV_CLK_PERIC1_SPI_MS_CTRL,
	DIV_CLK_PERIC1_USI07_SPI_I2C,
	DIV_CLK_PERIC1_USI08_SPI_I2C,
	DIV_CLK_PERIC2_I2C,
	DIV_CLK_PERIC2_SPI_MS_CTRL,
	DIV_CLK_PERIC2_UART_DBG,
	DIV_CLK_PERIC2_USI00_SPI_I2C,
	DIV_CLK_PERIC2_USI01_SPI_I2C,
};
enum clk_id cmucal_vclk_vdd_mif[] = {
	PLL_MIF_MAIN,
	PLL_MIF_SUB,
};
enum clk_id cmucal_vclk_vdd_g3d[] = {
	PLL_G3D,
};
enum clk_id cmucal_vclk_vdd_cam[] = {
	DIV_CLK_BYRP_NOCP,
	DIV_CLK_CSIS_NOCP,
	DIV_CLK_CSTAT_NOCP,
	DIV_CLK_MCFP_NOCP,
	DIV_CLK_NOCL2A_NOCP,
	DIV_CLK_RGBP_NOCP,
	DIV_CLK_YUVP_NOCP,
};
enum clk_id cmucal_vclk_vdd_cpucl2[] = {
	PLL_CPUCL2,
};
enum clk_id cmucal_vclk_vdd_cpucl0[] = {
	PLL_CPUCL0,
	PLL_DSU,
	DIV_CLK_CPUCL0_DBG_PCLKDBG,
};
enum clk_id cmucal_vclk_vdd_cpucl1[] = {
	PLL_CPUCL1,
};
enum clk_id cmucal_vclk_vdd_alive[] = {
	DIV_CLK_ALIVE_NOC,
	CLKALIVE_CMGP_NOC,
	CLKALIVE_DBGCORE_NOC,
	MUX_CLK_ALIVE_TIMER,
	DIV_CLK_HSI0_EUSB,
	CLKALIVE_UFD_NOC,
	MUX_CLKALIVE_UFD_NOC,
	CLKALIVE_VTS_NOC,
	MUX_CLKALIVE_VTS_NOC,
	CLKALIVE_CHUB_NOC,
	MUX_CLKALIVE_CHUB_NOC,
};
enum clk_id cmucal_vclk_vdd_npu[] = {
	DIV_CLK_AUD_CPU_PCLKDBG,
	DIV_CLK_AUD_CPU_ACLK,
	MUX_CLK_AUD_CPU,
	DIV_CLK_AUD_NOCP,
	DIV_CLK_AUD_SERIAL_LIF_CORE,
	DIV_CLK_AUD_NOC,
	MUX_CLK_AUD_NOC,
	CLKAUD_HSI0_NOC,
	MUX_CLK_AUD_DSIF,
	DIV_CLK_AUD_AUDIF,
	PLL_AUD,
	DIV_CLK_DNC_NOCP,
	DIV_CLK_GNPU_NOCP,
	DIV_CLK_GNPUP_NOCP,
	DIV_CLK_SDMA_NOCP,
	DIV_CLK_DPUB_NOCP,
	DIV_CLK_DPUF0_NOCP,
	DIV_CLK_DPUF1_NOCP,
};
enum clk_id cmucal_vclk_vdd_int_cmu[] = {
	CLKCMU_PERIC0_NOC,
	MUX_CLKCMU_PERIC0_NOC,
	CLKCMU_PERIC1_NOC,
	MUX_CLKCMU_PERIC1_NOC,
	CLKCMU_HSI1_NOC,
	MUX_CLKCMU_HSI1_NOC,
	CLKCMU_MFC0_WFD,
	MUX_CLKCMU_MFC0_WFD,
	DIV_CLKCMU_DPUF0_ALT,
	MUX_CLKCMU_DPUF0_ALT,
	DIV_CLKCMU_DPUF0,
	MUX_CLKCMU_DPUF0,
	DIV_CLKCMU_DPUB_ALT,
	MUX_CLKCMU_DPUB_ALT,
	DIV_CLKCMU_DPUB,
	MUX_CLKCMU_DPUB,
	CLKCMU_CSIS_DCPHY,
	MUX_CLKCMU_CSIS_DCPHY,
	CLKCMU_SSP_NOC,
	MUX_CLKCMU_SSP_NOC,
	CLKCMU_PERIC2_NOC,
	MUX_CLKCMU_PERIC2_NOC,
	CLKCMU_HSI1_UFS_EMBD,
	MUX_CLKCMU_HSI1_UFS_EMBD,
	CLKCMU_G3D_NOCP,
	MUX_CLKCMU_G3D_NOCP,
	DIV_CLKCMU_DPUF1_ALT,
	MUX_CLKCMU_DPUF1_ALT,
	DIV_CLKCMU_DPUF1,
	MUX_CLKCMU_DPUF1,
	DIV_CLKCMU_CIS_CLK7,
	MUX_CLKCMU_HSI1_PCIE,
	CLKCMU_ALIVE_NOC,
	MUX_CLKCMU_ALIVE_NOC,
	CLKCMU_PERIC0_IP0,
	MUX_CLKCMU_PERIC0_IP0,
	CLKCMU_PERIC1_IP0,
	MUX_CLKCMU_PERIC1_IP0,
	CLKCMU_HSI0_NOC,
	MUX_CLKCMU_HSI0_NOC,
	CLKCMU_CSIS_OIS_MCU,
	MUX_CLKCMU_CSIS_OIS_MCU,
	CLKCMU_HSI0_DPGTC,
	MUX_CLKCMU_HSI0_DPGTC,
	CLKCMU_AUD_NOC,
	MUX_CLKCMU_AUD_NOC,
	CLKCMU_PERIC0_IP1,
	MUX_CLKCMU_PERIC0_IP1,
	CLKCMU_PERIC1_IP1,
	MUX_CLKCMU_PERIC1_IP1,
	CLKCMU_PERIC2_IP0,
	MUX_CLKCMU_PERIC2_IP0,
	CLKCMU_PERIC2_IP1,
	MUX_CLKCMU_PERIC2_IP1,
	CLKCMU_DPUB_DSIM,
	DIV_CLKCMU_AUD_AUDIF0_SM,
	DIV_CLKCMU_AUD_AUDIF0,
	MUX_CLKCMU_AUD_AUDIF0,
	DIV_CLKCMU_AUD_CPU_SM,
	MUX_CLKCMU_AUD_CPU,
	CLKCMU_MIF_NOCP,
	MUX_CLKCMU_MIF_NOCP,
	DIV_CLKCMU_M2M_NOC_SM,
	DIV_CLKCMU_M2M_NOC,
	MUX_CLKCMU_M2M_NOC,
	MUX_CLKCMU_HSI1_MMC_CARD,
	PLL_MMC,
	DIV_CLKCMU_G3D_SWITCH,
	DIV_CLKCMU_G3D_SWITCH_SM,
	MUX_CLKCMU_G3D_SWITCH,
	CLKCMU_DRCP_NOC,
	MUX_CLKCMU_DRCP_NOC,
	DIV_CLKCMU_DSP_NOC,
	DIV_CLKCMU_DSP_NOC_SM,
	MUX_CLKCMU_DSP_NOC,
	CLKCMU_BYRP_NOC,
	MUX_CLKCMU_BYRP_NOC,
	DIV_CLKCMU_GNPU_NOC,
	DIV_CLKCMU_GNPU_NOC_SM,
	MUX_CLKCMU_GNPU_NOC,
	CLKCMU_MFC1_MFC1,
	MUX_CLKCMU_MFC1_MFC1,
	CLKCMU_MFC0_MFC0,
	MUX_CLKCMU_MFC0_MFC0,
	CLKCMU_MCSC_NOC,
	MUX_CLKCMU_MCSC_NOC,
	CLKCMU_MCSC_GDC,
	MUX_CLKCMU_MCSC_GDC,
	CLKCMU_MCFP_NOC,
	MUX_CLKCMU_MCFP_NOC,
	CLKCMU_LME_NOC,
	MUX_CLKCMU_LME_NOC,
	CLKCMU_LME_FRC,
	MUX_CLKCMU_LME_FRC,
	CLKCMU_YUVP_NOC,
	MUX_CLKCMU_YUVP_NOC,
	CLKCMU_RGBP_NOC,
	MUX_CLKCMU_RGBP_NOC,
	CLKCMU_CSIS_NOC,
	MUX_CLKCMU_CSIS_NOC,
	DIV_CLKCMU_NOCL0_NOC,
	DIV_CLKCMU_NOCL0_NOC_SM,
	MUX_CLKCMU_NOCL0_NOC,
	CLKCMU_CSTAT_NOC,
	MUX_CLKCMU_CSTAT_NOC,
	DIV_CLKCMU_DNC_NOC,
	DIV_CLKCMU_DNC_NOC_SM,
	MUX_CLKCMU_DNC_NOC,
	DIV_CLKCMU_SDMA_NOC_SM,
	DIV_CLKCMU_SDMA_NOC,
	MUX_CLKCMU_SDMA_NOC,
	DIV_CLKCMU_CPUCL0_SWITCH,
	DIV_CLKCMU_CPUCL0_SWITCH_SM,
	MUX_CLKCMU_CPUCL0_SWITCH,
	DIV_CLKCMU_CPUCL1_SWITCH,
	DIV_CLKCMU_CPUCL1_SWITCH_SM,
	MUX_CLKCMU_CPUCL1_SWITCH,
	DIV_CLKCMU_CPUCL2_SWITCH,
	DIV_CLKCMU_CPUCL2_SWITCH_SM,
	MUX_CLKCMU_CPUCL2_SWITCH,
	DIV_CLKCMU_GNPUP_NOC,
	DIV_CLKCMU_GNPUP_NOC_SM,
	MUX_CLKCMU_GNPUP_NOC,
	DIV_CLKCMU_DSU_SWITCH,
	DIV_CLKCMU_DSU_SWITCH_SM,
	MUX_CLKCMU_DSU_SWITCH,
	PLL_SHARED_MIF,
	CLKCMU_CPUCL0_NOCP,
	MUX_CLKCMU_CPUCL0_NOCP,
	DIV_CP_SHARED2_CLK_SM,
	DIV_CP_SHARED2_CLK,
	CP_SHARED0_CLK,
	MUX_CP_SHARED0_CLK,
	MUX_CP_SHARED1_CLK,
	CLKCMU_PERIS_GIC,
	MUX_CLKCMU_PERIS_GIC,
	DIV_CLKCMU_NOCL1B_NOC0_SM,
	DIV_CLKCMU_NOCL1B_NOC0,
	MUX_CLKCMU_NOCL1B_NOC0,
	DIV_CLKCMU_NOCL1A_NOC_SM,
	DIV_CLKCMU_NOCL1A_NOC,
	MUX_CLKCMU_NOCL1A_NOC,
	CLKCMU_NOCL1B_NOC1,
	MUX_CLKCMU_NOCL1B_NOC1,
	DIV_CLKCMU_CPUCL0_DBG_NOC,
	DIV_CLKCMU_CPUCL0_DBG_NOC_SM,
	MUX_CLKCMU_CPUCL0_DBG_NOC,
	DIV_CLKCMU_NOCL2A_NOC_SM,
	DIV_CLKCMU_NOCL2A_NOC,
	MUX_CLKCMU_NOCL2A_NOC,
	CLKCMU_CMU_BOOST_MIF,
	MUX_CMU_CMUREF,
	CLKCMU_CMU_BOOST,
	CLKCMU_CMU_BOOST_CPU,
	CLKCMU_CMU_BOOST_CAM,
	DIV_CLKCMU_AUD_AUDIF1_SM,
	DIV_CLKCMU_AUD_AUDIF1,
	MUX_CLKCMU_AUD_AUDIF1,
};
/* SPECIAL VCLK -> Clock Node List */
enum clk_id cmucal_vclk_mux_chub_timer[] = {
	MUX_CHUB_TIMER,
};
enum clk_id cmucal_vclk_mux_cpucl0_cmuref[] = {
	MUX_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost_cpu[] = {
	MUX_CLKCMU_CMU_BOOST_CPU,
};
enum clk_id cmucal_vclk_mux_cpucl1_cmuref[] = {
	MUX_CPUCL1_CMUREF,
};
enum clk_id cmucal_vclk_mux_cpucl2_cmuref[] = {
	MUX_CPUCL2_CMUREF,
};
enum clk_id cmucal_vclk_mux_dsu_cmuref[] = {
	MUX_DSU_CMUREF,
};
enum clk_id cmucal_vclk_mux_clk_dsu_powerip[] = {
	MUX_CLK_DSU_POWERIP,
};
enum clk_id cmucal_vclk_mux_clk_hsi0_usb32drd[] = {
	MUX_CLK_HSI0_USB32DRD,
	MUX_CLK_HSI0_RTCCLK,
};
enum clk_id cmucal_vclk_clkcmu_hsi0_usb32drd[] = {
	CLKCMU_HSI0_USB32DRD,
	MUX_CLKCMU_HSI0_USB32DRD,
};
enum clk_id cmucal_vclk_mux_mif_cmuref[] = {
	MUX_MIF_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost_mif[] = {
	MUX_CLKCMU_CMU_BOOST_MIF,
};
enum clk_id cmucal_vclk_mux_nocl0_cmuref[] = {
	MUX_NOCL0_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost[] = {
	MUX_CLKCMU_CMU_BOOST,
};
enum clk_id cmucal_vclk_mux_nocl1a_cmuref[] = {
	MUX_NOCL1A_CMUREF,
};
enum clk_id cmucal_vclk_mux_nocl1b_cmuref[] = {
	MUX_NOCL1B_CMUREF,
};
enum clk_id cmucal_vclk_mux_nocl2a_cmuref[] = {
	MUX_NOCL2A_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost_cam[] = {
	MUX_CLKCMU_CMU_BOOST_CAM,
};
enum clk_id cmucal_vclk_mux_clkcmu_dpub_dsim[] = {
	MUX_CLKCMU_DPUB_DSIM,
};
enum clk_id cmucal_vclk_clkcmu_hsi0_dposc[] = {
	CLKCMU_HSI0_DPOSC,
	MUX_CLKCMU_HSI0_DPOSC,
};
enum clk_id cmucal_vclk_div_clk_alive_spmi[] = {
	DIV_CLK_ALIVE_SPMI,
	MUX_CLK_ALIVE_SPMI,
};
enum clk_id cmucal_vclk_div_clk_alive_dbgcore_uart[] = {
	DIV_CLK_ALIVE_DBGCORE_UART,
	MUX_CLK_ALIVE_DBGCORE_UART,
};
enum clk_id cmucal_vclk_div_clk_alive_pmu_sub[] = {
	DIV_CLK_ALIVE_PMU_SUB,
	MUX_CLK_ALIVE_PMU_SUB,
};
enum clk_id cmucal_vclk_div_clk_aud_dsif[] = {
	DIV_CLK_AUD_DSIF,
	DIV_CLK_AUD_UAIF0,
	MUX_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF1,
	MUX_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF2,
	MUX_CLK_AUD_UAIF2,
	DIV_CLK_AUD_UAIF3,
	MUX_CLK_AUD_UAIF3,
	DIV_CLK_AUD_CNT,
	DIV_CLK_AUD_UAIF4,
	MUX_CLK_AUD_UAIF4,
	DIV_CLK_AUD_UAIF5,
	MUX_CLK_AUD_UAIF5,
	DIV_CLK_AUD_UAIF6,
	MUX_CLK_AUD_UAIF6,
	DIV_CLK_AUD_PCMC,
	MUX_CLK_AUD_PCMC,
};
enum clk_id cmucal_vclk_clkcmu_aud_cpu[] = {
	CLKCMU_AUD_CPU,
	DIV_CLKCMU_AUD_CPU,
};
enum clk_id cmucal_vclk_clkcmu_aud_audif0[] = {
	CLKCMU_AUD_AUDIF0,
};
enum clk_id cmucal_vclk_clkcmu_aud_audif1[] = {
	CLKCMU_AUD_AUDIF1,
};
enum clk_id cmucal_vclk_div_clk_aud_serial_lif[] = {
	DIV_CLK_AUD_SERIAL_LIF,
	MUX_CLK_AUD_SERIAL_LIF,
	MUX_CLK_AUD_SERIAL_LIF_CORE,
};
enum clk_id cmucal_vclk_clk_aud_mclk[] = {
	CLK_AUD_MCLK,
};
enum clk_id cmucal_vclk_div_clk_chub_dmailbox[] = {
	DIV_CLK_CHUB_DMAILBOX,
	MUX_CLK_CHUB_DMAILBOX_CORE,
};
enum clk_id cmucal_vclk_clkalive_chub_peri[] = {
	CLKALIVE_CHUB_PERI,
	MUX_CLKALIVE_CHUB_PERI,
};
enum clk_id cmucal_vclk_div_clk_chub_usi0[] = {
	DIV_CLK_CHUB_USI0,
	MUX_CLK_CHUB_USI0,
};
enum clk_id cmucal_vclk_div_clk_chub_usi1[] = {
	DIV_CLK_CHUB_USI1,
	MUX_CLK_CHUB_USI1,
};
enum clk_id cmucal_vclk_div_clk_chub_usi3[] = {
	DIV_CLK_CHUB_USI3,
	MUX_CLK_CHUB_USI3,
};
enum clk_id cmucal_vclk_div_clk_chub_usi2[] = {
	DIV_CLK_CHUB_USI2,
	MUX_CLK_CHUB_USI2,
};
enum clk_id cmucal_vclk_div_clk_chub_spi_ms_ctrl[] = {
	DIV_CLK_CHUB_SPI_MS_CTRL,
	MUX_CLK_CHUB_SPI_MS_CTRL,
};
enum clk_id cmucal_vclk_div_clk_chub_spi_i2c0[] = {
	DIV_CLK_CHUB_SPI_I2C0,
	MUX_CLK_CHUB_SPI_I2C0,
};
enum clk_id cmucal_vclk_div_clk_chub_spi_i2c1[] = {
	DIV_CLK_CHUB_SPI_I2C1,
	MUX_CLK_CHUB_SPI_I2C1,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi4[] = {
	DIV_CLK_CMGP_USI4,
	MUX_CLK_CMGP_USI4,
};
enum clk_id cmucal_vclk_clkalive_cmgp_peri[] = {
	CLKALIVE_CMGP_PERI,
	MUX_CLKALIVE_CMGP_PERI,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi1[] = {
	DIV_CLK_CMGP_USI1,
	MUX_CLK_CMGP_USI1,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi0[] = {
	DIV_CLK_CMGP_USI0,
	MUX_CLK_CMGP_USI0,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi2[] = {
	DIV_CLK_CMGP_USI2,
	MUX_CLK_CMGP_USI2,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi3[] = {
	DIV_CLK_CMGP_USI3,
	MUX_CLK_CMGP_USI3,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi5[] = {
	DIV_CLK_CMGP_USI5,
	MUX_CLK_CMGP_USI5,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi6[] = {
	DIV_CLK_CMGP_USI6,
	MUX_CLK_CMGP_USI6,
};
enum clk_id cmucal_vclk_div_clk_cmgp_i3c[] = {
	DIV_CLK_CMGP_I3C,
	MUX_CLK_CMGP_I3C,
};
enum clk_id cmucal_vclk_div_clk_cmgp_spi_ms_ctrl[] = {
	DIV_CLK_CMGP_SPI_MS_CTRL,
	MUX_CLK_CMGP_SPI_MS_CTRL,
};
enum clk_id cmucal_vclk_div_clk_cmgp_spi_i2c0[] = {
	DIV_CLK_CMGP_SPI_I2C0,
	MUX_CLK_CMGP_SPI_I2C0,
};
enum clk_id cmucal_vclk_div_clk_cmgp_spi_i2c1[] = {
	DIV_CLK_CMGP_SPI_I2C1,
	MUX_CLK_CMGP_SPI_I2C1,
};
enum clk_id cmucal_vclk_clkcmu_hpm[] = {
	CLKCMU_HPM,
	MUX_CLKCMU_HPM,
};
enum clk_id cmucal_vclk_div_clkcmu_cis_clk0[] = {
	DIV_CLKCMU_CIS_CLK0,
	MUX_CLKCMU_CIS_CLK0,
};
enum clk_id cmucal_vclk_div_clkcmu_cis_clk1[] = {
	DIV_CLKCMU_CIS_CLK1,
	MUX_CLKCMU_CIS_CLK1,
};
enum clk_id cmucal_vclk_div_clkcmu_cis_clk2[] = {
	DIV_CLKCMU_CIS_CLK2,
	MUX_CLKCMU_CIS_CLK2,
};
enum clk_id cmucal_vclk_div_clkcmu_cis_clk3[] = {
	DIV_CLKCMU_CIS_CLK3,
	MUX_CLKCMU_CIS_CLK3,
};
enum clk_id cmucal_vclk_div_clkcmu_cis_clk4[] = {
	DIV_CLKCMU_CIS_CLK4,
	MUX_CLKCMU_CIS_CLK4,
};
enum clk_id cmucal_vclk_div_clkcmu_cis_clk5[] = {
	DIV_CLKCMU_CIS_CLK5,
	MUX_CLKCMU_CIS_CLK5,
};
enum clk_id cmucal_vclk_div_clkcmu_cis_clk6[] = {
	DIV_CLKCMU_CIS_CLK6,
	MUX_CLKCMU_CIS_CLK6,
};
enum clk_id cmucal_vclk_mux_clkcmu_cis_clk7[] = {
	MUX_CLKCMU_CIS_CLK7,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_core_1[] = {
	DIV_CLK_CPUCL1_CORE_1,
	DIV_CLK_CPUCL1_CORE_2,
};
enum clk_id cmucal_vclk_div_clk_cpucl2_htu[] = {
	DIV_CLK_CPUCL2_HTU,
};
enum clk_id cmucal_vclk_clk_dnc_add_ch_clk[] = {
	CLK_DNC_ADD_CH_CLK,
};
enum clk_id cmucal_vclk_clk_dsu_str_dem_clk[] = {
	CLK_DSU_STR_DEM_CLK,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi04[] = {
	DIV_CLK_PERIC0_USI04,
	MUX_CLK_PERIC0_USI04,
};
enum clk_id cmucal_vclk_mux_clk_peric0_i2c[] = {
	MUX_CLK_PERIC0_I2C,
};
enum clk_id cmucal_vclk_mux_clk_peric1_uart_bt[] = {
	MUX_CLK_PERIC1_UART_BT,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi07[] = {
	DIV_CLK_PERIC1_USI07,
	MUX_CLK_PERIC1_USI07,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi08[] = {
	DIV_CLK_PERIC1_USI08,
	MUX_CLK_PERIC1_USI08,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi09[] = {
	DIV_CLK_PERIC1_USI09,
	MUX_CLK_PERIC1_USI09,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi10[] = {
	DIV_CLK_PERIC1_USI10,
	MUX_CLK_PERIC1_USI10,
};
enum clk_id cmucal_vclk_mux_clk_peric1_spi_ms_ctrl[] = {
	MUX_CLK_PERIC1_SPI_MS_CTRL,
};
enum clk_id cmucal_vclk_mux_clk_peric1_usi07_spi_i2c[] = {
	MUX_CLK_PERIC1_USI07_SPI_I2C,
};
enum clk_id cmucal_vclk_mux_clk_peric1_usi08_spi_i2c[] = {
	MUX_CLK_PERIC1_USI08_SPI_I2C,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi00[] = {
	DIV_CLK_PERIC2_USI00,
	MUX_CLK_PERIC2_USI00,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi01[] = {
	DIV_CLK_PERIC2_USI01,
	MUX_CLK_PERIC2_USI01,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi02[] = {
	DIV_CLK_PERIC2_USI02,
	MUX_CLK_PERIC2_USI02,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi03[] = {
	DIV_CLK_PERIC2_USI03,
	MUX_CLK_PERIC2_USI03,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi05[] = {
	DIV_CLK_PERIC2_USI05,
	MUX_CLK_PERIC2_USI05,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi06[] = {
	DIV_CLK_PERIC2_USI06,
	MUX_CLK_PERIC2_USI06,
};
enum clk_id cmucal_vclk_mux_clk_peric2_spi_ms_ctrl[] = {
	MUX_CLK_PERIC2_SPI_MS_CTRL,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi11[] = {
	DIV_CLK_PERIC2_USI11,
	MUX_CLK_PERIC2_USI11,
};
enum clk_id cmucal_vclk_mux_clk_peric2_uart_dbg[] = {
	MUX_CLK_PERIC2_UART_DBG,
};
enum clk_id cmucal_vclk_mux_clk_peric2_usi00_spi_i2c[] = {
	MUX_CLK_PERIC2_USI00_SPI_I2C,
};
enum clk_id cmucal_vclk_mux_clk_peric2_usi01_spi_i2c[] = {
	MUX_CLK_PERIC2_USI01_SPI_I2C,
};
enum clk_id cmucal_vclk_div_clk_vts_serial_lif[] = {
	DIV_CLK_VTS_SERIAL_LIF,
	MUX_CLK_VTS_SERIAL_LIF,
	DIV_CLK_VTS_SERIAL_LIF_CORE,
	MUX_CLK_VTS_SERIAL_LIF_CORE,
};
enum clk_id cmucal_vclk_clkcmu_vts_dmic[] = {
	CLKCMU_VTS_DMIC,
	MUX_CLKCMU_VTS_DMIC,
};
enum clk_id cmucal_vclk_div_clk_vts_dmailbox_cclk[] = {
	DIV_CLK_VTS_DMAILBOX_CCLK,
	MUX_CLK_VTS_DMAILBOX_CCLK,
};
enum clk_id cmucal_vclk_clk_yuvp_add_ch_clk[] = {
	CLK_YUVP_ADD_CH_CLK,
};
enum clk_id cmucal_vclk_mux_clkalive_gnss_noc[] = {
	MUX_CLKALIVE_GNSS_NOC,
};
enum clk_id cmucal_vclk_clkcmu_dnc_noc[] = {
	CLKCMU_DNC_NOC,
};
enum clk_id cmucal_vclk_clkalive_dnc_noc[] = {
	CLKALIVE_DNC_NOC,
	MUX_CLKALIVE_DNC_NOC,
};
enum clk_id cmucal_vclk_mux_clkcmu_mif_switch[] = {
	MUX_CLKCMU_MIF_SWITCH,
};
/* COMMON VCLK -> Clock Node List */
enum clk_id cmucal_vclk_blk_cmu[] = {
	CLKCMU_NOCL0_NOC,
	CLKCMU_DSP_NOC,
	CLKCMU_GNPU_NOC,
	DIV_CP_SHARED0_CLK_SM,
	DIV_CP_SHARED0_CLK,
	CLKCMU_GNPUP_NOC,
	CLKCMU_PERIS_NOC,
	MUX_CLKCMU_PERIS_NOC,
	CLKCMU_CPUCL0_DBG_NOC,
	CLKCMU_HSI1_MMC_CARD,
	DIV_CLKCMU_HSI1_MMC_CARD,
	DIV_CLKCMU_HSI1_MMC_CARD_SM,
	CLKCMU_SDMA_NOC,
	CP_SHARED1_CLK,
	CLKCMU_M2M_NOC,
	CLKCMU_DPUB_NOC,
	CLKCMU_DPUF0_NOC,
	CLKCMU_DPUF1_NOC,
	CLKCMU_NOCL2A_NOC,
	CLKCMU_NOCL1B_NOC0,
	CLKCMU_NOCL1A_NOC,
	PLL_SHARED0,
	CP_SHARED2_CLK,
	MUX_CP_SHARED2_CLK,
	PLL_SHARED1,
	CP_HISPEEDY_CLK,
	DIV_CP_HISPEEDY_CLK,
	DIV_CP_HISPEEDY_CLK_SM,
	MUX_CP_HISPEEDY_CLK,
	PLL_SHARED2,
	PLL_SHARED3,
	PLL_SHARED4,
};
enum clk_id cmucal_vclk_blk_s2d[] = {
	MUX_CLK_S2D_CORE,
	PLL_MIF_S2D,
};
enum clk_id cmucal_vclk_blk_alive[] = {
	MUX_CLKALIVE_CMGP_NOC,
	MUX_CLK_ALIVE_NOC,
	MUX_CLKALIVE_DBGCORE_NOC,
	CLKALIVE_GNPU_NOC,
	MUX_CLKALIVE_GNPU_NOC,
	CLKALIVE_SDMA_NOC,
	MUX_CLKALIVE_SDMA_NOC,
	CLKALIVE_GNPUP_NOC,
	MUX_CLKALIVE_GNPUP_NOC,
};
enum clk_id cmucal_vclk_blk_aud[] = {
	MUX_CLK_AUD_SCLK,
	MUX_CLK_AUD_AUDIF,
};
enum clk_id cmucal_vclk_blk_chub[] = {
	DIV_CLK_CHUB_I2C,
	MUX_CLK_CHUB_I2C,
	DIV_CLK_CHUB_NOC,
};
enum clk_id cmucal_vclk_blk_cmgp[] = {
	DIV_CLK_CMGP_I2C,
	MUX_CLK_CMGP_I2C,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	CLK_CPUCL0_DDD,
	MUX_CLK_CPUCL0_IDLECLKDOWN,
	MUX_CLK_CPUCL0_DELAYMUX,
	MUX_CLK_CPUCL0_DELAYCHAIN,
	MUX_CLK_CPUCL0_POWERIP,
	CLK_CPUCL0_ADD_CH_CLK,
};
enum clk_id cmucal_vclk_blk_cpucl1[] = {
	CLK_CPUCL1_DDD_0,
	MUX_CLK_CPUCL1_IDLECLKDOWN_0,
	MUX_CLK_CPUCL1_DELAYMUX_0,
	MUX_CLK_CPUCL1_DELAYCHAIN_0,
	DIV_CLK_CPUCL1_HTU,
	MUX_CLK_CPUCL1_POWERIP,
	CLK_CPUCL1_DDD_1,
	MUX_CLK_CPUCL1_IDLECLKDOWN_1,
	MUX_CLK_CPUCL1_DELAYMUX_1,
	MUX_CLK_CPUCL1_DELAYCHAIN_1,
	CLK_CPUCL1_DDD_2,
	MUX_CLK_CPUCL1_IDLECLKDOWN_2,
	MUX_CLK_CPUCL1_DELAYMUX_2,
	MUX_CLK_CPUCL1_DELAYCHAIN_2,
	CLK_CPUCL1_ADD_CH_CLK,
};
enum clk_id cmucal_vclk_blk_cpucl2[] = {
	CLK_CPUCL2_DDD,
	MUX_CLK_CPUCL2_IDLECLKDOWN,
	MUX_CLK_CPUCL2_DELAYMUX,
	MUX_CLK_CPUCL2_DELAYCHAIN,
	MUX_CLK_CPUCL2_POWERIP,
	CLK_CPUCL2_ADD_CH_CLK,
};
enum clk_id cmucal_vclk_blk_dnc[] = {
	MUX_CLK_DNC_NOC,
};
enum clk_id cmucal_vclk_blk_dsu[] = {
	DIV_CLK_CLUSTER_ACLK,
	DIV_CLK_CLUSTER_PCLK,
	DIV_CLK_CLUSTER_PERIPHCLK,
	DIV_CLK_CLUSTER_ATCLK,
	CLK_DSU_DDD,
	MUX_CLK_DSU_IDLECLKDOWN,
	MUX_CLK_DSU_DELAYMUX,
	MUX_CLK_DSU_DELAYCHAIN,
};
enum clk_id cmucal_vclk_blk_g3dcore[] = {
	CLK_G3D_DDD,
	MUX_CLK_G3D_CORE,
	MUX_CLK_G3D_DELAYMUX,
	MUX_CLK_G3D_DELAYCHAIN,
	CLK_G3D_ADD_CH_CLK,
};
enum clk_id cmucal_vclk_blk_gnpu[] = {
	MUX_CLK_GNPU_NOC,
};
enum clk_id cmucal_vclk_blk_gnpup[] = {
	MUX_CLK_GNPUP_NOC,
};
enum clk_id cmucal_vclk_blk_hsi0[] = {
	MUX_CLK_HSI0_NOC,
};
enum clk_id cmucal_vclk_blk_peric1[] = {
	MUX_CLK_PERIC1_I2C,
};
enum clk_id cmucal_vclk_blk_peric2[] = {
	MUX_CLK_PERIC2_I2C,
};
enum clk_id cmucal_vclk_blk_sdma[] = {
	MUX_CLK_SDMA_NOC,
};
enum clk_id cmucal_vclk_blk_ufd[] = {
	MUX_CLK_UFD_NOC,
};
enum clk_id cmucal_vclk_blk_vts[] = {
	DIV_CLK_VTS_DMIC_AHB,
	MUX_CLK_VTS_DMIC_AHB,
	MUX_CLKVTS_AUD_DMIC1,
	DIV_CLKVTS_AUD_DMIC1,
	DIV_CLK_VTS_DMIC_IF_DIV2,
	DIV_CLK_VTS_DMIC_IF,
	DIV_CLKVTS_AUD_DMIC0,
	MUX_CLK_VTS_DMIC_PAD,
	DIV_CLK_VTS_NOC,
	DIV_CLK_VTS_CPU,
};
enum clk_id cmucal_vclk_blk_cpucl0_glb[] = {
	DIV_CLK_CPUCL0_DBG_NOC,
};
enum clk_id cmucal_vclk_blk_dsp[] = {
	DIV_CLK_DSP_NOCP,
};
enum clk_id cmucal_vclk_blk_m2m[] = {
	DIV_CLK_M2M_NOCP,
};
enum clk_id cmucal_vclk_blk_mfc0[] = {
	DIV_CLK_MFC0_NOCP,
};
enum clk_id cmucal_vclk_blk_mfc1[] = {
	DIV_CLK_MFC1_NOCP,
};
enum clk_id cmucal_vclk_blk_ssp[] = {
	DIV_CLK_SSP_NOCP,
};
/* GATE VCLK -> Clock Node List */
enum clk_id cmucal_vclk_ip_alive_cmu_alive[] = {
	CLK_BLK_ALIVE_UID_ALIVE_CMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_alive[] = {
	CLK_BLK_ALIVE_UID_APBIF_PMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_rtc[] = {
	CLK_BLK_ALIVE_UID_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_intcomb_vgpio2ap[] = {
	CLK_BLK_ALIVE_UID_APBIF_INTCOMB_VGPIO2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_intcomb_vgpio2apm[] = {
	CLK_BLK_ALIVE_UID_APBIF_INTCOMB_VGPIO2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_intcomb_vgpio2pmu[] = {
	CLK_BLK_ALIVE_UID_APBIF_INTCOMB_VGPIO2PMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_top_rtc[] = {
	CLK_BLK_ALIVE_UID_TOP_RTC_IPCLKPORT_PCLK,
	CLK_BLK_ALIVE_UID_TOP_RTC_IPCLKPORT_OSCCLK,
};
enum clk_id cmucal_vclk_ip_clkmon[] = {
	CLK_BLK_ALIVE_UID_CLKMON_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dbgcore_uart[] = {
	CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK,
	CLK_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dtzpc_alive[] = {
	CLK_BLK_ALIVE_UID_DTZPC_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration[] = {
	CLK_BLK_ALIVE_UID_GREBEINTEGRATION_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_hw_scandump_clkstop_ctrl[] = {
	CLK_BLK_ALIVE_UID_HW_SCANDUMP_CLKSTOP_CTRL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_spmi_master_pmic[] = {
	CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_I_PCLK,
	CLK_BLK_ALIVE_UID_SPMI_MASTER_PMIC_IPCLKPORT_I_IPCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_modem[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_MI_C_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_vts[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_MI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_apm[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_MI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_c_cmgp[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_SI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_apm[] = {
	CLK_BLK_ALIVE_UID_LH_AXI_SI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_lp_vts[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_SI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_ap[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_APM_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_cp[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_APM_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_AP_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp_s[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_AP_CP_S_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_dbgcore[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_AP_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pmu_intr_gen[] = {
	CLK_BLK_ALIVE_UID_PMU_INTR_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_rom_crc32_host[] = {
	CLK_BLK_ALIVE_UID_ROM_CRC32_HOST_IPCLKPORT_ACLK,
	CLK_BLK_ALIVE_UID_ROM_CRC32_HOST_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sweeper_p_alive[] = {
	CLK_BLK_ALIVE_UID_SWEEPER_P_ALIVE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_alive[] = {
	CLK_BLK_ALIVE_UID_SYSREG_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_alive[] = {
	CLK_BLK_ALIVE_UID_VGEN_LITE_ALIVE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_wdt_alive[] = {
	CLK_BLK_ALIVE_UID_WDT_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_alive[] = {
	CLK_BLK_ALIVE_UID_XIU_DP_ALIVE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_chub_rtc[] = {
	CLK_BLK_ALIVE_UID_CHUB_RTC_IPCLKPORT_PCLK,
	CLK_BLK_ALIVE_UID_CHUB_RTC_IPCLKPORT_OSCCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_chub[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_MI_C_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_gnss[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_MI_C_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_lp_chub[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_SI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_chub[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_APM_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_gnss[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_APM_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_vts[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_APM_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_chub[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_AP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_gnss[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_AP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_chub[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_CP_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_cp_gnss[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_CP_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_gnss_chub[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_GNSS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_shared_sram[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_SHARED_SRAM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_vts_chub[] = {
	CLK_BLK_ALIVE_UID_MAILBOX_VTS_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ppu_alive_cpucl0[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_SI_PPU_ALIVE_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_intmem[] = {
	CLK_BLK_ALIVE_UID_INTMEM_IPCLKPORT_ACLK,
	CLK_BLK_ALIVE_UID_INTMEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pmu[] = {
	CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_ACLK,
	CLK_BLK_ALIVE_UID_PMU_IPCLKPORT_CLKIN_PMU_SUB,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ip_apm[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_SI_IP_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_id_dbgcore[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_MI_ID_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_alivednc[] = {
	CLK_BLK_ALIVE_UID_SLH_AXI_SI_P_ALIVEDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_spc_alive[] = {
	CLK_BLK_ALIVE_UID_SPC_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apm_dma[] = {
	CLK_BLK_ALIVE_UID_APM_DMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_alive[] = {
	CLK_BLK_ALIVE_UID_APBIF_GPIO_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mct_alive[] = {
	CLK_BLK_ALIVE_UID_MCT_ALIVE_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_dft_alive[] = {
	CLK_BLK_ALIVE_UID_DFT_ALIVE_IPCLKPORT_SCAN_BIST_CLK_PMU,
};
enum clk_id cmucal_vclk_ip_aud_cmu_aud[] = {
	CLK_BLK_AUD_UID_AUD_CMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_abox[] = {
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_CNT,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_DSIF,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF0,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF1,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF3,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF4,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF5,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF6,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_C2A0_CLK,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_C2A1_CLK,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_CA32,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_DAP,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_PCMC_CLK,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_XCLK0,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_XCLK1,
	CLK_BLK_AUD_UID_ABOX_IPCLKPORT_XCLK2,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud[] = {
	CLK_BLK_AUD_UID_AD_APB_SMMU_AUD_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud_ns1[] = {
	CLK_BLK_AUD_UID_AD_APB_SMMU_AUD_NS1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud_s[] = {
	CLK_BLK_AUD_UID_AD_APB_SMMU_AUD_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_axi_us_32to128[] = {
	CLK_BLK_AUD_UID_AXI_US_32TO128_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_audvts[] = {
	CLK_BLK_AUD_UID_BAAW_D_AUDVTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_aud[] = {
	CLK_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_hsi0aud[] = {
	CLK_BLK_AUD_UID_SLH_AXI_MI_LD_HSI0AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_aud[] = {
	CLK_BLK_AUD_UID_SLH_AXI_MI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_qdi_si_d_aud[] = {
	CLK_BLK_AUD_UID_LH_QDI_SI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ld_audvts[] = {
	CLK_BLK_AUD_UID_SLH_AXI_SI_LD_AUDVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud0[] = {
	CLK_BLK_AUD_UID_MAILBOX_AUD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud1[] = {
	CLK_BLK_AUD_UID_MAILBOX_AUD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud2[] = {
	CLK_BLK_AUD_UID_MAILBOX_AUD2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud3[] = {
	CLK_BLK_AUD_UID_MAILBOX_AUD3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peri_axi_asb[] = {
	CLK_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKM,
	CLK_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_aud[] = {
	CLK_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_ACLK,
	CLK_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_aud[] = {
	CLK_BLK_AUD_UID_SMMU_AUD_IPCLKPORT_CLK_S1,
	CLK_BLK_AUD_UID_SMMU_AUD_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_aud[] = {
	CLK_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_aud[] = {
	CLK_BLK_AUD_UID_TREX_AUD_IPCLKPORT_CLK,
	CLK_BLK_AUD_UID_TREX_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_aud[] = {
	CLK_BLK_AUD_UID_WDT_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ld_audhsi0[] = {
	CLK_BLK_AUD_UID_SLH_AXI_SI_LD_AUDHSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_aud[] = {
	CLK_BLK_AUD_UID_VGEN_LITE_AUD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_lite[] = {
	CLK_BLK_AUD_UID_AD_APB_VGEN_LITE_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_serial_lif[] = {
	CLK_BLK_AUD_UID_SERIAL_LIF_IPCLKPORT_CLK,
	CLK_BLK_AUD_UID_SERIAL_LIF_IPCLKPORT_BCLK,
	CLK_BLK_AUD_UID_SERIAL_LIF_IPCLKPORT_ACLK,
	CLK_BLK_AUD_UID_SERIAL_LIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dftmux_aud[] = {
	CLK_BLK_AUD_UID_DFTMUX_AUD_IPCLKPORT_CODEC_MCLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_aud[] = {
	CLK_BLK_AUD_UID_SLH_ASTL_SI_G_PPMU_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud0[] = {
	CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_CLK,
	CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK,
	CLK_BLK_AUD_UID_DMIC_AUD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud1[] = {
	CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK,
	CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_DIV2_CLK,
	CLK_BLK_AUD_UID_DMIC_AUD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud2[] = {
	CLK_BLK_AUD_UID_DMIC_AUD2_IPCLKPORT_DMIC_AUD_CLK,
	CLK_BLK_AUD_UID_DMIC_AUD2_IPCLKPORT_DMIC_AUD_DIV2_CLK,
	CLK_BLK_AUD_UID_DMIC_AUD2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_byrp_cmu_byrp[] = {
	CLK_BLK_BYRP_UID_BYRP_CMU_BYRP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_byrp[] = {
	CLK_BLK_BYRP_UID_AD_APB_BYRP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_byrp[] = {
	CLK_BLK_BYRP_UID_D_TZPC_BYRP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_byrp[] = {
	CLK_BLK_BYRP_UID_LH_AXI_SI_D_BYRP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_byrp[] = {
	CLK_BLK_BYRP_UID_PPMU_BYRP_IPCLKPORT_ACLK,
	CLK_BLK_BYRP_UID_PPMU_BYRP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_byrp[] = {
	CLK_BLK_BYRP_UID_SYSMMU_D_BYRP_IPCLKPORT_CLK_S1,
	CLK_BLK_BYRP_UID_SYSMMU_D_BYRP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_byrp[] = {
	CLK_BLK_BYRP_UID_SYSREG_BYRP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_byrp[] = {
	CLK_BLK_BYRP_UID_XIU_D_BYRP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_byrp[] = {
	CLK_BLK_BYRP_UID_SLH_AXI_MI_P_BYRP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf_byrprgbp[] = {
	CLK_BLK_BYRP_UID_LH_AST_SI_OTF_BYRPRGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sipu_byrp[] = {
	CLK_BLK_BYRP_UID_SIPU_BYRP_IPCLKPORT_CLK,
	CLK_BLK_BYRP_UID_SIPU_BYRP_IPCLKPORT_CLK_C2S_STRP,
	CLK_BLK_BYRP_UID_SIPU_BYRP_IPCLKPORT_CLK_C2S_STRP,
	CLK_BLK_BYRP_UID_SIPU_BYRP_IPCLKPORT_CLK_C2S_BYR,
	CLK_BLK_BYRP_UID_SIPU_BYRP_IPCLKPORT_CLK_C2S_BYR,
};
enum clk_id cmucal_vclk_ip_vgen_lite_byrp[] = {
	CLK_BLK_BYRP_UID_VGEN_LITE_BYRP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_byrp[] = {
	CLK_BLK_BYRP_UID_SLH_ASTL_SI_G_PPMU_BYRP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_chub_cmu_chub[] = {
	CLK_BLK_CHUB_UID_CHUB_CMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_p_chub[] = {
	CLK_BLK_CHUB_UID_BPS_AXI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cm4_chub[] = {
	CLK_BLK_CHUB_UID_CM4_CHUB_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_dmailbox_chub[] = {
	CLK_BLK_CHUB_UID_DMAILBOX_CHUB_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_DMAILBOX_CHUB_IPCLKPORT_ACLK,
	CLK_BLK_CHUB_UID_DMAILBOX_CHUB_IPCLKPORT_CCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_lp_chub[] = {
	CLK_BLK_CHUB_UID_SLH_AXI_MI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_chub[] = {
	CLK_BLK_CHUB_UID_SLH_AXI_MI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_c_chub[] = {
	CLK_BLK_CHUB_UID_SLH_AXI_SI_C_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pwm_chub[] = {
	CLK_BLK_CHUB_UID_PWM_CHUB_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_sysreg_chub[] = {
	CLK_BLK_CHUB_UID_SYSREG_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer_chub[] = {
	CLK_BLK_CHUB_UID_TIMER_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_chub[] = {
	CLK_BLK_CHUB_UID_WDT_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_chub[] = {
	CLK_BLK_CHUB_UID_XIU_DP_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_i2c_chub[] = {
	CLK_BLK_CHUB_UID_I2C_CHUB_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_I2C_CHUB_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i3c_chub[] = {
	CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_I_PCLK,
	CLK_BLK_CHUB_UID_I3C_CHUB_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub0[] = {
	CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_USI_CHUB0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub1[] = {
	CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_USI_CHUB1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub3[] = {
	CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_USI_CHUB3_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_chub2[] = {
	CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_USI_CHUB2_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_lp_chub[] = {
	CLK_BLK_CHUB_UID_BPS_AXI_LP_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_chub_combine_wakeup_src[] = {
	CLK_BLK_CHUB_UID_APBIF_CHUB_COMBINE_WAKEUP_SRC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sweeper_c_chub[] = {
	CLK_BLK_CHUB_UID_SWEEPER_C_CHUB_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_chub[] = {
	CLK_BLK_CHUB_UID_APBIF_GPIO_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2ap[] = {
	CLK_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_combine_chub2apm[] = {
	CLK_BLK_CHUB_UID_SYSREG_COMBINE_CHUB2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_asyncinterrupt[] = {
	CLK_BLK_CHUB_UID_ASYNCINTERRUPT_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_dncchub[] = {
	CLK_BLK_CHUB_UID_SLH_AXI_MI_LD_DNCCHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_ld_dncchub[] = {
	CLK_BLK_CHUB_UID_BPS_AXI_LD_DNCCHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_chub_dnc[] = {
	CLK_BLK_CHUB_UID_MAILBOX_CHUB_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spi_i2c_chub0[] = {
	CLK_BLK_CHUB_UID_SPI_I2C_CHUB0_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_SPI_I2C_CHUB0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_spi_i2c_chub1[] = {
	CLK_BLK_CHUB_UID_SPI_I2C_CHUB1_IPCLKPORT_PCLK,
	CLK_BLK_CHUB_UID_SPI_I2C_CHUB1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_spi_multi_slv_q_ctrl_chub[] = {
	CLK_BLK_CHUB_UID_SPI_MULTI_SLV_Q_CTRL_CHUB_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_chub_alv[] = {
	CLK_BLK_CHUB_UID_CHUB_ALV_IPCLKPORT_UPMU_CHUB_ACLK,
	CLK_BLK_CHUB_UID_CHUB_ALV_IPCLKPORT_VGEN_PCLK,
	CLK_BLK_CHUB_UID_CHUB_ALV_IPCLKPORT_BAAW_C_PCLK,
	CLK_BLK_CHUB_UID_CHUB_ALV_IPCLKPORT_D_TZPC_PCLK,
	CLK_BLK_CHUB_UID_CHUB_ALV_IPCLKPORT_RSTNSYNCH_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_upmu_chub[] = {
	CLK_BLK_CHUB_UID_APBIF_UPMU_CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_cmgp[] = {
	CLK_BLK_CMGP_UID_APBIF_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cmgp[] = {
	CLK_BLK_CMGP_UID_D_TZPC_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spi_i2c_cmgp0[] = {
	CLK_BLK_CMGP_UID_SPI_I2C_CMGP0_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_SPI_I2C_CMGP0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_spi_i2c_cmgp1[] = {
	CLK_BLK_CMGP_UID_SPI_I2C_CMGP1_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_SPI_I2C_CMGP1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp2[] = {
	CLK_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp3[] = {
	CLK_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i3c_cmgp[] = {
	CLK_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_PCLK,
	CLK_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_c_cmgp[] = {
	CLK_BLK_CMGP_UID_SLH_AXI_MI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2apm[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2cp[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_ap[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2PMU_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp0[] = {
	CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK,
	CLK_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp1[] = {
	CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK,
	CLK_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp2[] = {
	CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK,
	CLK_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp3[] = {
	CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2chub[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2CHUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2gnss[] = {
	CLK_BLK_CMGP_UID_SYSREG_CMGP2GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp4[] = {
	CLK_BLK_CMGP_UID_I2C_CMGP4_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_I2C_CMGP4_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp5[] = {
	CLK_BLK_CMGP_UID_I2C_CMGP5_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_I2C_CMGP5_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp6[] = {
	CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_I2C_CMGP6_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp4[] = {
	CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_USI_CMGP4_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp5[] = {
	CLK_BLK_CMGP_UID_USI_CMGP5_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_USI_CMGP5_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp6[] = {
	CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_PCLK,
	CLK_BLK_CMGP_UID_USI_CMGP6_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_spi_multi_slv_q_ctrl_cmgp[] = {
	CLK_BLK_CMGP_UID_SPI_MULTI_SLV_Q_CTRL_CMGP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_lp_cmgpufd[] = {
	CLK_BLK_CMGP_UID_SLH_AXI_SI_LP_CMGPUFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cmgp_i2c[] = {
	CLK_BLK_CMGP_UID_CMGP_I2C_IPCLKPORT_IPCLK,
	CLK_BLK_CMGP_UID_CMGP_I2C_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_ADD_CPUCL0_0_IPCLKPORT_CLK,
	CLK_BLK_CPUCL0_UID_ADD_CPUCL0_0_IPCLKPORT_CH_CLK,
};
enum clk_id cmucal_vclk_ip_busif_add_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_BUSIF_ADD_CPUCL0_0_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL0_UID_BUSIF_ADD_CPUCL0_0_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL0_UID_BUSIF_ADD_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_BUSIF_STR_CPUCL0_0_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL0_UID_BUSIF_STR_CPUCL0_0_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL0_UID_BUSIF_STR_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_HTU_CPUCL0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_IPCLKPORT_COMPLEX0CLK,
	CLK_BLK_CPUCL0_UID_CPUCL0_IPCLKPORT_COMPLEX1CLK,
};
enum clk_id cmucal_vclk_ip_str_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_STR_CPUCL0_0_IPCLKPORT_DEM_CLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cluster0[] = {
	CLK_BLK_CPUCL0_GLB_UID_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_cpucl0_glb_cmu_cpucl0_glb[] = {
	CLK_BLK_CPUCL0_GLB_UID_CPUCL0_GLB_CMU_CPUCL0_GLB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0_0[] = {
	CLK_BLK_CPUCL0_GLB_UID_HPM_CPUCL0_0_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0_1[] = {
	CLK_BLK_CPUCL0_GLB_UID_HPM_CPUCL0_1_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0_2[] = {
	CLK_BLK_CPUCL0_GLB_UID_HPM_CPUCL0_2_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_apb_async_p_cssys[] = {
	CLK_BLK_CPUCL0_GLB_UID_APB_ASYNC_P_CSSYS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_bps_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_BPS_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpm_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_BUSIF_HPM_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cssys[] = {
	CLK_BLK_CPUCL0_GLB_UID_CSSYS_IPCLKPORT_PCLKDBG,
	CLK_BLK_CPUCL0_GLB_UID_CSSYS_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_D_TZPC_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_it_cluster0[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_IT_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_t_bdu[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_T_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_g_dbgcore[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ig_cssys[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_IG_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ig_dbgcore[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_IG_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ig_etr[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_IG_ETR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ig_stm[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_IG_STM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_MI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_g_cssys[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_AXI_SI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ig_cssys[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_SI_IG_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ig_dbgcore[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_SI_IG_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ig_etr[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_SI_IG_ETR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_secjtag[] = {
	CLK_BLK_CPUCL0_GLB_UID_SECJTAG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_TREX_CPUCL0_IPCLKPORT_CLK,
	CLK_BLK_CPUCL0_GLB_UID_TREX_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_cssys[] = {
	CLK_BLK_CPUCL0_GLB_UID_XIU_DP_CSSYS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_XIU_P_CPUCL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ig_stm[] = {
	CLK_BLK_CPUCL0_GLB_UID_SLH_AXI_SI_IG_STM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pmu_pcsm_pm[] = {
	CLK_BLK_CPUCL0_GLB_UID_PMU_PCSM_PM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_ddc_cpucl0_0[] = {
	CLK_BLK_CPUCL0_GLB_UID_BUSIF_DDC_CPUCL0_0_IPCLKPORT_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_busif_ddc_cpucl0_1[] = {
	CLK_BLK_CPUCL0_GLB_UID_BUSIF_DDC_CPUCL0_1_IPCLKPORT_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_it_ddcdsu[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_IT_DDCDSU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_t_ddcg3d[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_T_DDCG3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_it_ddclit[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_IT_DDCLIT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_it_ddcmid0[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_IT_DDCMID0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_it_ddcmid1[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_IT_DDCMID1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_it_ddcmid2[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_IT_DDCMID2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mi_it_ddcbig[] = {
	CLK_BLK_CPUCL0_GLB_UID_LH_ATB_MI_IT_DDCBIG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cfm_cpucl0[] = {
	CLK_BLK_CPUCL0_GLB_UID_CFM_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_cpucl0_1[] = {
	CLK_BLK_CPUCL1_UID_ADD_CPUCL0_1_IPCLKPORT_CLK,
	CLK_BLK_CPUCL1_UID_ADD_CPUCL0_1_IPCLKPORT_CH_CLK,
};
enum clk_id cmucal_vclk_ip_busif_add_cpucl0_1[] = {
	CLK_BLK_CPUCL1_UID_BUSIF_ADD_CPUCL0_1_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL1_UID_BUSIF_ADD_CPUCL0_1_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL1_UID_BUSIF_ADD_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_1[] = {
	CLK_BLK_CPUCL1_UID_BUSIF_STR_CPUCL0_1_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL1_UID_BUSIF_STR_CPUCL0_1_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL1_UID_BUSIF_STR_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl1_0[] = {
	CLK_BLK_CPUCL1_UID_HTU_CPUCL1_0_IPCLKPORT_I_CLK,
	CLK_BLK_CPUCL1_UID_HTU_CPUCL1_0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl1_cmu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_CMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl1_1[] = {
	CLK_BLK_CPUCL1_UID_HTU_CPUCL1_1_IPCLKPORT_I_CLK,
	CLK_BLK_CPUCL1_UID_HTU_CPUCL1_1_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl1_2[] = {
	CLK_BLK_CPUCL1_UID_HTU_CPUCL1_2_IPCLKPORT_I_CLK,
	CLK_BLK_CPUCL1_UID_HTU_CPUCL1_2_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_str_cpucl0_1[] = {
	CLK_BLK_CPUCL1_UID_STR_CPUCL0_1_IPCLKPORT_DEM_CLK,
};
enum clk_id cmucal_vclk_ip_add_cpucl0_2[] = {
	CLK_BLK_CPUCL2_UID_ADD_CPUCL0_2_IPCLKPORT_CLK,
	CLK_BLK_CPUCL2_UID_ADD_CPUCL0_2_IPCLKPORT_CH_CLK,
};
enum clk_id cmucal_vclk_ip_busif_add_cpucl0_2[] = {
	CLK_BLK_CPUCL2_UID_BUSIF_ADD_CPUCL0_2_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL2_UID_BUSIF_ADD_CPUCL0_2_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL2_UID_BUSIF_ADD_CPUCL0_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_2[] = {
	CLK_BLK_CPUCL2_UID_BUSIF_STR_CPUCL0_2_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL2_UID_BUSIF_STR_CPUCL0_2_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL2_UID_BUSIF_STR_CPUCL0_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl2[] = {
	CLK_BLK_CPUCL2_UID_HTU_CPUCL2_IPCLKPORT_I_CLK,
	CLK_BLK_CPUCL2_UID_HTU_CPUCL2_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl2_cmu_cpucl2[] = {
	CLK_BLK_CPUCL2_UID_CPUCL2_CMU_CPUCL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_str_cpucl0_2[] = {
	CLK_BLK_CPUCL2_UID_STR_CPUCL0_2_IPCLKPORT_DEM_CLK,
};
enum clk_id cmucal_vclk_ip_mipi_phy_link_wrap[] = {
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS4,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS0,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS1,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS2,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS3,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS5,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_ACLK_CSIS6,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_PCLK_CSIS0,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_PCLK_CSIS1,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_PCLK_CSIS2,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_PCLK_CSIS3,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_PCLK_CSIS4,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_PCLK_CSIS5,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_CSIS_LINK_PCLK_CSIS6,
	CLK_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_I_FD_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_csis[] = {
	CLK_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S2,
	CLK_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_csis_cmu_csis[] = {
	CLK_BLK_CSIS_UID_CSIS_CMU_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis_wdma[] = {
	CLK_BLK_CSIS_UID_AD_APB_CSIS_WDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_csis_pdp[] = {
	CLK_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_DMA,
	CLK_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_MCB_EBUF_BNS,
	CLK_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_VOTF0,
	CLK_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_ACLK_VOTF1,
	CLK_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_I_PDP_CLK,
	CLK_BLK_CSIS_UID_CSIS_PDP_IPCLKPORT_I_PDP_C2CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_csis[] = {
	CLK_BLK_CSIS_UID_D_TZPC_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_ois_mcu_top[] = {
	CLK_BLK_CSIS_UID_AD_AXI_OIS_MCU_TOP_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_xiu_p_csis[] = {
	CLK_BLK_CSIS_UID_XIU_P_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_csis[] = {
	CLK_BLK_CSIS_UID_LH_AXI_SI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_csis[] = {
	CLK_BLK_CSIS_UID_LH_AXI_SI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d2_csis[] = {
	CLK_BLK_CSIS_UID_LH_AXI_SI_D2_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ois_mcu_top[] = {
	CLK_BLK_CSIS_UID_OIS_MCU_TOP_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0[] = {
	CLK_BLK_CSIS_UID_PPMU_D0_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_PPMU_D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1[] = {
	CLK_BLK_CSIS_UID_PPMU_D1_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_PPMU_D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d2[] = {
	CLK_BLK_CSIS_UID_PPMU_D2_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_PPMU_D2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_wdma0[] = {
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA0_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_wdma1[] = {
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA1_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_wdma2[] = {
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA2_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_wdma3[] = {
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA3_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_d0[] = {
	CLK_BLK_CSIS_UID_QE_PDP_D0_IPCLKPORT_PCLK,
	CLK_BLK_CSIS_UID_QE_PDP_D0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_d1[] = {
	CLK_BLK_CSIS_UID_QE_PDP_D1_IPCLKPORT_PCLK,
	CLK_BLK_CSIS_UID_QE_PDP_D1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_d2[] = {
	CLK_BLK_CSIS_UID_QE_PDP_D2_IPCLKPORT_PCLK,
	CLK_BLK_CSIS_UID_QE_PDP_D2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_d3[] = {
	CLK_BLK_CSIS_UID_QE_PDP_D3_IPCLKPORT_PCLK,
	CLK_BLK_CSIS_UID_QE_PDP_D3_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_csis[] = {
	CLK_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S1,
	CLK_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_csis[] = {
	CLK_BLK_CSIS_UID_SYSMMU_D2_CSIS_IPCLKPORT_CLK_S1,
	CLK_BLK_CSIS_UID_SYSMMU_D2_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_csis[] = {
	CLK_BLK_CSIS_UID_SYSREG_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d3_csis[] = {
	CLK_BLK_CSIS_UID_XIU_D3_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d4_csis[] = {
	CLK_BLK_CSIS_UID_XIU_D4_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_mipi_phy[] = {
	CLK_BLK_CSIS_UID_AD_APB_MIPI_PHY_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf0_csiscstat[] = {
	CLK_BLK_CSIS_UID_LH_AST_SI_OTF0_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf1_csiscstat[] = {
	CLK_BLK_CSIS_UID_LH_AST_SI_OTF1_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf2_csiscstat[] = {
	CLK_BLK_CSIS_UID_LH_AST_SI_OTF2_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf3_csiscstat[] = {
	CLK_BLK_CSIS_UID_LH_AST_SI_OTF3_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d3[] = {
	CLK_BLK_CSIS_UID_PPMU_D3_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_PPMU_D3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_wdma4[] = {
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA4_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_QE_CSIS_WDMA4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d3_csis[] = {
	CLK_BLK_CSIS_UID_LH_AXI_SI_D3_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_csisperic1[] = {
	CLK_BLK_CSIS_UID_SLH_AXI_SI_P_CSISPERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d3_csis[] = {
	CLK_BLK_CSIS_UID_SYSMMU_D3_CSIS_IPCLKPORT_CLK_S1,
	CLK_BLK_CSIS_UID_SYSMMU_D3_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_d4[] = {
	CLK_BLK_CSIS_UID_PPMU_D4_IPCLKPORT_ACLK,
	CLK_BLK_CSIS_UID_PPMU_D4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d4_csis[] = {
	CLK_BLK_CSIS_UID_SYSMMU_D4_CSIS_IPCLKPORT_CLK_S1,
	CLK_BLK_CSIS_UID_SYSMMU_D4_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d0[] = {
	CLK_BLK_CSIS_UID_VGEN_LITE_D0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d1[] = {
	CLK_BLK_CSIS_UID_VGEN_LITE_D1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d4_csis[] = {
	CLK_BLK_CSIS_UID_LH_AXI_SI_D4_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_csis[] = {
	CLK_BLK_CSIS_UID_SLH_ASTL_SI_G_PPMU_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g_ppmu_csis[] = {
	CLK_BLK_CSIS_UID_SIU_G_PPMU_CSIS_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_si_otf_csisdnc[] = {
	CLK_BLK_CSIS_UID_SLH_AST_SI_OTF_CSISDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_si_otf_csisufd[] = {
	CLK_BLK_CSIS_UID_SLH_AST_SI_OTF_CSISUFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_lp_ufdcsis[] = {
	CLK_BLK_CSIS_UID_SLH_AXI_MI_LP_UFDCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_csis[] = {
	CLK_BLK_CSIS_UID_SLH_AXI_MI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cstat_cmu_cstat[] = {
	CLK_BLK_CSTAT_UID_CSTAT_CMU_CSTAT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cstat[] = {
	CLK_BLK_CSTAT_UID_D_TZPC_CSTAT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_cstat0[] = {
	CLK_BLK_CSTAT_UID_AD_APB_CSTAT0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_cstat[] = {
	CLK_BLK_CSTAT_UID_SLH_AXI_MI_P_CSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cstat[] = {
	CLK_BLK_CSTAT_UID_PPMU_CSTAT_IPCLKPORT_ACLK,
	CLK_BLK_CSTAT_UID_PPMU_CSTAT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_cstat[] = {
	CLK_BLK_CSTAT_UID_SYSMMU_D_CSTAT_IPCLKPORT_CLK_S1,
	CLK_BLK_CSTAT_UID_SYSMMU_D_CSTAT_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_cstat[] = {
	CLK_BLK_CSTAT_UID_LH_AXI_SI_D_CSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cstat[] = {
	CLK_BLK_CSTAT_UID_SYSREG_CSTAT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_cstat[] = {
	CLK_BLK_CSTAT_UID_XIU_D_CSTAT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf0_csiscstat[] = {
	CLK_BLK_CSTAT_UID_LH_AST_MI_OTF0_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf1_csiscstat[] = {
	CLK_BLK_CSTAT_UID_LH_AST_MI_OTF1_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf2_csiscstat[] = {
	CLK_BLK_CSTAT_UID_LH_AST_MI_OTF2_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf3_csiscstat[] = {
	CLK_BLK_CSTAT_UID_LH_AST_MI_OTF3_CSISCSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sipu_cstat[] = {
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2RD,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2RD,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2DS,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2DS,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2CDS0,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2CDS0,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2CDS1,
	CLK_BLK_CSTAT_UID_SIPU_CSTAT_IPCLKPORT_I_CLK_C2CDS1,
};
enum clk_id cmucal_vclk_ip_vgen_lite_cstat0[] = {
	CLK_BLK_CSTAT_UID_VGEN_LITE_CSTAT0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_cstat1[] = {
	CLK_BLK_CSTAT_UID_VGEN_LITE_CSTAT1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_cstat2[] = {
	CLK_BLK_CSTAT_UID_VGEN_LITE_CSTAT2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_cstat[] = {
	CLK_BLK_CSTAT_UID_SLH_ASTL_SI_G_PPMU_CSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_AHB_BUSMATRIX_DBGCORE_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_GREBEINTEGRATION_DBGCORE_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ip_apm[] = {
	CLK_BLK_DBGCORE_UID_SLH_AXI_MI_IP_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_g_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_SLH_AXI_SI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_g_scan2dram[] = {
	CLK_BLK_DBGCORE_UID_SLH_AXI_SI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_id_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_SLH_AXI_SI_ID_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_SYSREG_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_WDT_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xhb_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_XHB_DBGCORE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dbgcore_cmu_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_DBGCORE_CMU_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mdis_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_MDIS_DBGCORE_IPCLKPORT_I_PCLK,
	CLK_BLK_DBGCORE_UID_MDIS_DBGCORE_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dbgcore_core[] = {
	CLK_BLK_DBGCORE_UID_SYSREG_DBGCORE_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_D_TZPC_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_s2d_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_APBIF_S2D_DBGCORE_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_asyncahbmaster_dbgcore[] = {
	CLK_BLK_DBGCORE_UID_ASYNCAHBMASTER_DBGCORE_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_dsp0dnc_shmem[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_DSP0DNC_SHMEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_dsp1dnc_shmem[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_DSP1DNC_SHMEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_gnpu0dnc_shmem[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_GNPU0DNC_SHMEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_gnpu1dnc_shmem[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_GNPU1DNC_SHMEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dnc_cmu_dnc[] = {
	CLK_BLK_DNC_UID_DNC_CMU_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_cmdq_gnpu0[] = {
	CLK_BLK_DNC_UID_SLH_AXI_MI_D_CMDQ_GNPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_dsp0dnc_sfr[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_DSP0DNC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_dsp1dnc_sfr[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_DSP1DNC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_cmdq_gnpu1[] = {
	CLK_BLK_DNC_UID_SLH_AXI_MI_D_CMDQ_GNPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_dnc[] = {
	CLK_BLK_DNC_UID_SLH_AXI_MI_P_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_dsp1dnc_cache[] = {
	CLK_BLK_DNC_UID_SLH_AXI_MI_LD_DSP1DNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_gnpu0[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_D1_GNPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_lp_ipdnc[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LP_IPDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_gnpu1[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_D1_GNPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_lp_ipdnc[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_LP_IPDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu3[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_MMU3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu2[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_MMU2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu1[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_MMU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_adm_dap_dnc[] = {
	CLK_BLK_DNC_UID_ADM_DAP_DNC_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu0[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_MMU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_rq_gnpu0[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_D_RQ_GNPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_rq_gnpu1[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_D_RQ_GNPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_dncdsp1_dma[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_LD_DNCDSP1_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_dsp0dnc_cache[] = {
	CLK_BLK_DNC_UID_SLH_AXI_MI_LD_DSP0DNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_gnpu0[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_D0_GNPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_dncdsp1_sfr[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_LD_DNCDSP1_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_gnpu1[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_D0_GNPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_dnc[] = {
	CLK_BLK_DNC_UID_IP_DNC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_dncdsp0_dma[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_LD_DNCDSP0_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dnc[] = {
	CLK_BLK_DNC_UID_SYSREG_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_dncdsp0_sfr[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_LD_DNCDSP0_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dnc[] = {
	CLK_BLK_DNC_UID_D_TZPC_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data3[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data2[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data1[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data0[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data7[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA7_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_ctrl_gnpu1[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_D_CTRL_GNPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data6[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA6_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_ctrl_gnpu0[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_D_CTRL_GNPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data5[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA5_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data4[] = {
	CLK_BLK_DNC_UID_LH_AXI_MI_LD_SDMADNC_DATA4_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ipdnc[] = {
	CLK_BLK_DNC_UID_PPMU_IPDNC_IPCLKPORT_ACLK,
	CLK_BLK_DNC_UID_PPMU_IPDNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_sdma0[] = {
	CLK_BLK_DNC_UID_PPMU_SDMA0_IPCLKPORT_ACLK,
	CLK_BLK_DNC_UID_PPMU_SDMA0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_sdma1[] = {
	CLK_BLK_DNC_UID_PPMU_SDMA1_IPCLKPORT_ACLK,
	CLK_BLK_DNC_UID_PPMU_SDMA1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_sdma2[] = {
	CLK_BLK_DNC_UID_PPMU_SDMA2_IPCLKPORT_ACLK,
	CLK_BLK_DNC_UID_PPMU_SDMA2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_sdma3[] = {
	CLK_BLK_DNC_UID_PPMU_SDMA3_IPCLKPORT_ACLK,
	CLK_BLK_DNC_UID_PPMU_SDMA3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_ipdnc[] = {
	CLK_BLK_DNC_UID_SYSMMU_IPDNC_IPCLKPORT_CLK_S1,
	CLK_BLK_DNC_UID_SYSMMU_IPDNC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_sdma0[] = {
	CLK_BLK_DNC_UID_SYSMMU_SDMA0_IPCLKPORT_CLK_S1,
	CLK_BLK_DNC_UID_SYSMMU_SDMA0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_sdma1[] = {
	CLK_BLK_DNC_UID_SYSMMU_SDMA1_IPCLKPORT_CLK_S1,
	CLK_BLK_DNC_UID_SYSMMU_SDMA1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_sdma2[] = {
	CLK_BLK_DNC_UID_SYSMMU_SDMA2_IPCLKPORT_CLK_S1,
	CLK_BLK_DNC_UID_SYSMMU_SDMA2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_sdma3[] = {
	CLK_BLK_DNC_UID_SYSMMU_SDMA3_IPCLKPORT_CLK_S1,
	CLK_BLK_DNC_UID_SYSMMU_SDMA3_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_trex_d_dnc[] = {
	CLK_BLK_DNC_UID_TREX_D_DNC_IPCLKPORT_ACLK,
	CLK_BLK_DNC_UID_TREX_D_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbas_s1_ns_sdma0[] = {
	CLK_BLK_DNC_UID_APBAS_S1_NS_SDMA0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_busif_hpm_dnc[] = {
	CLK_BLK_DNC_UID_BUSIF_HPM_DNC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_dnc[] = {
	CLK_BLK_DNC_UID_HPM_DNC_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_htu_dnc[] = {
	CLK_BLK_DNC_UID_HTU_DNC_IPCLKPORT_I_CLK,
	CLK_BLK_DNC_UID_HTU_DNC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_ddd_dnc[] = {
	CLK_BLK_DNC_UID_BUSIF_DDD_DNC_IPCLKPORT_PCLK,
	CLK_BLK_DNC_UID_BUSIF_DDD_DNC_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_busif_add_dnc[] = {
	CLK_BLK_DNC_UID_BUSIF_ADD_DNC_IPCLKPORT_PCLK,
	CLK_BLK_DNC_UID_BUSIF_ADD_DNC_IPCLKPORT_CLK_CORE,
	CLK_BLK_DNC_UID_BUSIF_ADD_DNC_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_add_dnc[] = {
	CLK_BLK_DNC_UID_ADD_DNC_IPCLKPORT_CH_CLK,
	CLK_BLK_DNC_UID_ADD_DNC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ld_dncvts[] = {
	CLK_BLK_DNC_UID_SLH_AXI_SI_LD_DNCVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ld_dncchub[] = {
	CLK_BLK_DNC_UID_SLH_AXI_SI_LD_DNCCHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_dncchub[] = {
	CLK_BLK_DNC_UID_BAAW_DNCCHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_dncvts[] = {
	CLK_BLK_DNC_UID_BAAW_DNCVTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ddd_dnc[] = {
	CLK_BLK_DNC_UID_DDD_DNC_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_lp_dncsdma[] = {
	CLK_BLK_DNC_UID_LH_AXI_SI_LP_DNCSDMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g_ppmu_dnc[] = {
	CLK_BLK_DNC_UID_SIU_G_PPMU_DNC_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_dnc[] = {
	CLK_BLK_DNC_UID_SLH_ASTL_SI_G_PPMU_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_gnpu0[] = {
	CLK_BLK_DNC_UID_SLH_AXI_SI_P_GNPU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_gnpu1[] = {
	CLK_BLK_DNC_UID_SLH_AXI_SI_P_GNPU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_alivednc[] = {
	CLK_BLK_DNC_UID_SLH_AXI_MI_P_ALIVEDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_dnc[] = {
	CLK_BLK_DNC_UID_VGEN_DNC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_dnc[] = {
	CLK_BLK_DNC_UID_VGEN_LITE_DNC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_gnpup0[] = {
	CLK_BLK_DNC_UID_SLH_AXI_SI_P_GNPUP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_gnpup1[] = {
	CLK_BLK_DNC_UID_SLH_AXI_SI_P_GNPUP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_ufddnc[] = {
	CLK_BLK_DNC_UID_SLH_AXI_MI_LD_UFDDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_dnc[] = {
	CLK_BLK_DNC_UID_XIU_P_DNC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_mi_otf_csisdnc[] = {
	CLK_BLK_DNC_UID_SLH_AST_MI_OTF_CSISDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_glue_otf_csisdnc[] = {
	CLK_BLK_DNC_UID_SLH_AST_GLUE_OTF_CSISDNC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dpub_cmu_dpub[] = {
	CLK_BLK_DPUB_UID_DPUB_CMU_DPUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_decon_main[] = {
	CLK_BLK_DPUB_UID_AD_APB_DECON_MAIN_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dpub[] = {
	CLK_BLK_DPUB_UID_DPUB_IPCLKPORT_ACLK_DECON,
	CLK_BLK_DPUB_UID_DPUB_IPCLKPORT_ALVCLK_DSIM1,
	CLK_BLK_DPUB_UID_DPUB_IPCLKPORT_ALVCLK_DSIM2,
	CLK_BLK_DPUB_UID_DPUB_IPCLKPORT_ALVCLK_DSIM0,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpub[] = {
	CLK_BLK_DPUB_UID_D_TZPC_DPUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_dpub[] = {
	CLK_BLK_DPUB_UID_SLH_AXI_MI_P_DPUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dpub[] = {
	CLK_BLK_DPUB_UID_SYSREG_DPUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_upi_m0[] = {
	CLK_BLK_DPUB_UID_UPI_M0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dpuf0_cmu_dpuf0[] = {
	CLK_BLK_DPUF0_UID_DPUF0_CMU_DPUF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dpuf0_dma[] = {
	CLK_BLK_DPUF0_UID_AD_APB_DPUF0_DMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dpuf0[] = {
	CLK_BLK_DPUF0_UID_DPUF0_IPCLKPORT_ACLK_DPUF,
	CLK_BLK_DPUF0_UID_DPUF0_IPCLKPORT_ACLK_VOTF,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpuf0[] = {
	CLK_BLK_DPUF0_UID_D_TZPC_DPUF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_dpuf0[] = {
	CLK_BLK_DPUF0_UID_SLH_AXI_MI_P_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_dpuf0[] = {
	CLK_BLK_DPUF0_UID_LH_AXI_SI_D1_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf0d0[] = {
	CLK_BLK_DPUF0_UID_PPMU_DPUF0D0_IPCLKPORT_ACLK,
	CLK_BLK_DPUF0_UID_PPMU_DPUF0D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf0d1[] = {
	CLK_BLK_DPUF0_UID_PPMU_DPUF0D1_IPCLKPORT_ACLK,
	CLK_BLK_DPUF0_UID_PPMU_DPUF0D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf0d0[] = {
	CLK_BLK_DPUF0_UID_SYSMMU_DPUF0D0_IPCLKPORT_CLK_S2,
	CLK_BLK_DPUF0_UID_SYSMMU_DPUF0D0_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf0d1[] = {
	CLK_BLK_DPUF0_UID_SYSMMU_DPUF0D1_IPCLKPORT_CLK_S1,
	CLK_BLK_DPUF0_UID_SYSMMU_DPUF0D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_dpuf0[] = {
	CLK_BLK_DPUF0_UID_SYSREG_DPUF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_dpuf0[] = {
	CLK_BLK_DPUF0_UID_XIU_D0_DPUF0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_dpuf0[] = {
	CLK_BLK_DPUF0_UID_XIU_D1_DPUF0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_dpuf0[] = {
	CLK_BLK_DPUF0_UID_SIU_DPUF0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_dpuf1dpuf0[] = {
	CLK_BLK_DPUF0_UID_LH_AXI_MI_D0_DPUF1DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_dpuf1dpuf0[] = {
	CLK_BLK_DPUF0_UID_LH_AXI_MI_D1_DPUF1DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_dpuf0[] = {
	CLK_BLK_DPUF0_UID_SLH_ASTL_SI_G_PPMU_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d0_dpuf0[] = {
	CLK_BLK_DPUF0_UID_SLH_AXI_SI_D0_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpuf1_cmu_dpuf1[] = {
	CLK_BLK_DPUF1_UID_DPUF1_CMU_DPUF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dpuf1_dma[] = {
	CLK_BLK_DPUF1_UID_AD_APB_DPUF1_DMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dpuf1[] = {
	CLK_BLK_DPUF1_UID_DPUF1_IPCLKPORT_ACLK_DPUF,
	CLK_BLK_DPUF1_UID_DPUF1_IPCLKPORT_ACLK_VOTF,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpuf1[] = {
	CLK_BLK_DPUF1_UID_D_TZPC_DPUF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_dpuf1[] = {
	CLK_BLK_DPUF1_UID_SLH_AXI_MI_P_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf1d0[] = {
	CLK_BLK_DPUF1_UID_PPMU_DPUF1D0_IPCLKPORT_ACLK,
	CLK_BLK_DPUF1_UID_PPMU_DPUF1D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf1d1[] = {
	CLK_BLK_DPUF1_UID_PPMU_DPUF1D1_IPCLKPORT_ACLK,
	CLK_BLK_DPUF1_UID_PPMU_DPUF1D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf1d0[] = {
	CLK_BLK_DPUF1_UID_SYSMMU_DPUF1D0_IPCLKPORT_CLK_S1,
	CLK_BLK_DPUF1_UID_SYSMMU_DPUF1D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf1d1[] = {
	CLK_BLK_DPUF1_UID_SYSMMU_DPUF1D1_IPCLKPORT_CLK_S1,
	CLK_BLK_DPUF1_UID_SYSMMU_DPUF1D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_dpuf1[] = {
	CLK_BLK_DPUF1_UID_SYSREG_DPUF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_siu_dpuf1[] = {
	CLK_BLK_DPUF1_UID_SIU_DPUF1_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_dpuf1dpuf0[] = {
	CLK_BLK_DPUF1_UID_LH_AXI_SI_D0_DPUF1DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_dpuf1dpuf0[] = {
	CLK_BLK_DPUF1_UID_LH_AXI_SI_D1_DPUF1DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_dpuf1[] = {
	CLK_BLK_DPUF1_UID_SLH_ASTL_SI_G_PPMU_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_drcp_cmu_drcp[] = {
	CLK_BLK_DRCP_UID_DRCP_CMU_DRCP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_drcp[] = {
	CLK_BLK_DRCP_UID_AD_APB_DRCP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_drcp[] = {
	CLK_BLK_DRCP_UID_DRCP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d_drcp[] = {
	CLK_BLK_DRCP_UID_PPMU_D_DRCP_IPCLKPORT_PCLK,
	CLK_BLK_DRCP_UID_PPMU_D_DRCP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_drcp[] = {
	CLK_BLK_DRCP_UID_SYSMMU_D_DRCP_IPCLKPORT_CLK_S2,
	CLK_BLK_DRCP_UID_SYSMMU_D_DRCP_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_d_tzpc_drcp[] = {
	CLK_BLK_DRCP_UID_D_TZPC_DRCP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_drcp[] = {
	CLK_BLK_DRCP_UID_XIU_D_DRCP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_drcp[] = {
	CLK_BLK_DRCP_UID_SYSREG_DRCP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_drcp[] = {
	CLK_BLK_DRCP_UID_SLH_AXI_MI_P_DRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf_yuvpdrcp[] = {
	CLK_BLK_DRCP_UID_LH_AST_MI_OTF_YUVPDRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf_drcpmcsc[] = {
	CLK_BLK_DRCP_UID_LH_AST_SI_OTF_DRCPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_drcp[] = {
	CLK_BLK_DRCP_UID_LH_AXI_SI_D_DRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d_drcp[] = {
	CLK_BLK_DRCP_UID_VGEN_LITE_D_DRCP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_drcp[] = {
	CLK_BLK_DRCP_UID_SLH_ASTL_SI_G_PPMU_DRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_dspdnc_shmem[] = {
	CLK_BLK_DSP_UID_LH_AXI_SI_LD_DSPDNC_SHMEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dsp_cmu_dsp[] = {
	CLK_BLK_DSP_UID_DSP_CMU_DSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dsp[] = {
	CLK_BLK_DSP_UID_SYSREG_DSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dsp[] = {
	CLK_BLK_DSP_UID_D_TZPC_DSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_dsp[] = {
	CLK_BLK_DSP_UID_SLH_AXI_MI_P_DSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_dncdsp_dma[] = {
	CLK_BLK_DSP_UID_LH_AXI_MI_LD_DNCDSP_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_ld_dncdsp_sfr[] = {
	CLK_BLK_DSP_UID_LH_AXI_MI_LD_DNCDSP_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ld_dspdnc_cache[] = {
	CLK_BLK_DSP_UID_SLH_AXI_SI_LD_DSPDNC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_dspdnc_sfr[] = {
	CLK_BLK_DSP_UID_LH_AXI_SI_LD_DSPDNC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_dsp[] = {
	CLK_BLK_DSP_UID_IP_DSP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_strm_sdmadsp[] = {
	CLK_BLK_DSP_UID_LH_AST_MI_LD_STRM_SDMADSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cluster0[] = {
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_PCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_PERIPHCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_ATCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_PPUCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_GICCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_SCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_3[] = {
	CLK_BLK_DSU_UID_BUSIF_STR_CPUCL0_3_IPCLKPORT_CLK_CORE,
	CLK_BLK_DSU_UID_BUSIF_STR_CPUCL0_3_IPCLKPORT_CLK_CORE,
	CLK_BLK_DSU_UID_BUSIF_STR_CPUCL0_3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_iri_giccpu_cluster0[] = {
	CLK_BLK_DSU_UID_LH_AST_MI_LD_IRI_GICCPU_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_icc_cpugic_cluster0[] = {
	CLK_BLK_DSU_UID_LH_AST_SI_LD_ICC_CPUGIC_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_si_it_cluster0[] = {
	CLK_BLK_DSU_UID_LH_ATB_SI_IT_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrret_cluster0_0[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_0_IPCLKPORT_CLK,
	CLK_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrret_cluster0_1[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_1_IPCLKPORT_CLK,
	CLK_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrrun_cluster0_0[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_0_IPCLKPORT_CLK,
	CLK_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrrun_cluster0_1[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_1_IPCLKPORT_CLK,
	CLK_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ppu_alive_cpucl0[] = {
	CLK_BLK_DSU_UID_SLH_AXI_MI_PPU_ALIVE_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dsu_cmu_dsu[] = {
	CLK_BLK_DSU_UID_DSU_CMU_DSU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_dsu[] = {
	CLK_BLK_DSU_UID_HTU_DSU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_chi_si_d0_cluster0[] = {
	CLK_BLK_DSU_UID_LH_CHI_SI_D0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_chi_si_d1_cluster0[] = {
	CLK_BLK_DSU_UID_LH_CHI_SI_D1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gray2bin_cntvalueb[] = {
	CLK_BLK_DSU_UID_GRAY2BIN_CNTVALUEB_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_u_sync_ppuwakeup_cluster0[] = {
	CLK_BLK_DSU_UID_U_SYNC_PPUWAKEUP_CLUSTER0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ip_utility[] = {
	CLK_BLK_DSU_UID_SLH_AXI_MI_IP_UTILITY_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_utility[] = {
	CLK_BLK_DSU_UID_XIU_DP_UTILITY_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_cluster0_nocl0[] = {
	CLK_BLK_DSU_UID_SLH_AXI_SI_P_CLUSTER0_NOCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ip_utility[] = {
	CLK_BLK_DSU_UID_SLH_AXI_SI_IP_UTILITY_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_peripheral[] = {
	CLK_BLK_DSU_UID_XIU_DP_PERIPHERAL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_str_cpucl0_3[] = {
	CLK_BLK_DSU_UID_STR_CPUCL0_3_IPCLKPORT_DEM_CLK,
};
enum clk_id cmucal_vclk_ip_g3d_cmu_g3d[] = {
	CLK_BLK_G3D_UID_G3D_CMU_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmg3d[] = {
	CLK_BLK_G3D_UID_BUSIF_HPMG3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_int_g3d[] = {
	CLK_BLK_G3D_UID_SLH_AXI_SI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_g3d[] = {
	CLK_BLK_G3D_UID_HPM_G3D_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_sysreg_g3d[] = {
	CLK_BLK_G3D_UID_SYSREG_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g3d[] = {
	CLK_BLK_G3D_UID_D_TZPC_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_g3d[] = {
	CLK_BLK_G3D_UID_SLH_AXI_MI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cfm_g3d[] = {
	CLK_BLK_G3D_UID_CFM_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_bg3d_pwrctl[] = {
	CLK_BLK_G3D_UID_BG3D_PWRCTL_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_add_g3d[] = {
	CLK_BLK_G3DCORE_UID_ADD_G3D_IPCLKPORT_CLK,
	CLK_BLK_G3DCORE_UID_ADD_G3D_IPCLKPORT_CH_CLK,
};
enum clk_id cmucal_vclk_ip_add_apbif_g3d[] = {
	CLK_BLK_G3DCORE_UID_ADD_APBIF_G3D_IPCLKPORT_CLK_CORE,
	CLK_BLK_G3DCORE_UID_ADD_APBIF_G3D_IPCLKPORT_CLK_CORE,
	CLK_BLK_G3DCORE_UID_ADD_APBIF_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_g3d[] = {
	CLK_BLK_G3DCORE_UID_BUSIF_STR_G3D_IPCLKPORT_CLK_CORE,
	CLK_BLK_G3DCORE_UID_BUSIF_STR_G3D_IPCLKPORT_CLK_CORE,
	CLK_BLK_G3DCORE_UID_BUSIF_STR_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_g3d[] = {
	CLK_BLK_G3DCORE_UID_HTU_G3D_IPCLKPORT_I_PCLK,
	CLK_BLK_G3DCORE_UID_HTU_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_g3dcore_cmu_g3dcore[] = {
	CLK_BLK_G3DCORE_UID_G3DCORE_CMU_G3DCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpu[] = {
	CLK_BLK_G3DCORE_UID_GPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adm_dap_g_g3d[] = {
	CLK_BLK_G3DCORE_UID_ADM_DAP_G_G3D_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_busif_ddc_g3d[] = {
	CLK_BLK_G3DCORE_UID_BUSIF_DDC_G3D_IPCLKPORT_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_str_g3d[] = {
	CLK_BLK_G3DCORE_UID_STR_G3D_IPCLKPORT_DEM_CLK,
};
enum clk_id cmucal_vclk_ip_gnpu_cmu_gnpu[] = {
	CLK_BLK_GNPU_UID_GNPU_CMU_GNPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ip_npucore[] = {
	CLK_BLK_GNPU_UID_IP_NPUCORE_IPCLKPORT_CLK_CORE,
	CLK_BLK_GNPU_UID_IP_NPUCORE_IPCLKPORT_CLK_SRAM,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_ctrl_gnpu[] = {
	CLK_BLK_GNPU_UID_LH_AXI_MI_D_CTRL_GNPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_rq_gnpu[] = {
	CLK_BLK_GNPU_UID_LH_AXI_SI_D_RQ_GNPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au0[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au31[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU31_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au30[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU30_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au4[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU4_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au3[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au2[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au1[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au8[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU8_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au7[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU7_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au6[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU6_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au5[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU5_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au9[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU9_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_gnpu[] = {
	CLK_BLK_GNPU_UID_LH_AXI_MI_D1_GNPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au20[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU20_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au22[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU22_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au21[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU21_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au24[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU24_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au23[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU23_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au26[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU26_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au25[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU25_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au28[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU28_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au27[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU27_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au29[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU29_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_gnpu[] = {
	CLK_BLK_GNPU_UID_D_TZPC_GNPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu3[] = {
	CLK_BLK_GNPU_UID_LH_AST_MI_LD_GNPUP_BU3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu2[] = {
	CLK_BLK_GNPU_UID_LH_AST_MI_LD_GNPUP_BU2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_cu_rdata[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_CU_RDATA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_auloader[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AULOADER_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpup_rdreq[] = {
	CLK_BLK_GNPU_UID_LH_AST_MI_LD_GNPUP_RDREQ_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpup_cu_wch[] = {
	CLK_BLK_GNPU_UID_LH_AST_MI_LD_GNPUP_CU_WCH_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au11[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU11_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu1[] = {
	CLK_BLK_GNPU_UID_LH_AST_MI_LD_GNPUP_BU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au10[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu0[] = {
	CLK_BLK_GNPU_UID_LH_AST_MI_LD_GNPUP_BU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au13[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU13_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au12[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU12_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_gnpu[] = {
	CLK_BLK_GNPU_UID_SLH_AXI_MI_P_GNPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d_cmdq_gnpu[] = {
	CLK_BLK_GNPU_UID_SLH_AXI_SI_D_CMDQ_GNPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au15[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU15_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au14[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU14_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au17[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU17_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au16[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU16_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au19[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU19_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_au18[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_AU18_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_gnpu[] = {
	CLK_BLK_GNPU_UID_SYSREG_GNPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_gnpu[] = {
	CLK_BLK_GNPU_UID_LH_AXI_MI_D0_GNPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_npuc_setreg[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_NPUC_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpup_npuc_done[] = {
	CLK_BLK_GNPU_UID_LH_AST_MI_LD_GNPUP_NPUC_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_gnpudnc_shmem[] = {
	CLK_BLK_GNPU_UID_LH_AXI_SI_LD_GNPUDNC_SHMEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_gnpu_0[] = {
	CLK_BLK_GNPU_UID_XIU_D_GNPU_0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_gnpu_1[] = {
	CLK_BLK_GNPU_UID_XIU_D_GNPU_1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpu_sram_wrresp[] = {
	CLK_BLK_GNPU_UID_LH_AST_SI_LD_GNPU_SRAM_WRRESP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gnpup_cmu_gnpup[] = {
	CLK_BLK_GNPUP_UID_GNPUP_CMU_GNPUP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au3[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_cu_rdata[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_CU_RDATA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au4[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU4_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au5[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU5_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au6[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU6_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu1[] = {
	CLK_BLK_GNPUP_UID_LH_AST_SI_LD_GNPUP_BU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au7[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU7_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu2[] = {
	CLK_BLK_GNPUP_UID_LH_AST_SI_LD_GNPUP_BU2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au8[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU8_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_gnpup[] = {
	CLK_BLK_GNPUP_UID_D_TZPC_GNPUP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu3[] = {
	CLK_BLK_GNPUP_UID_LH_AST_SI_LD_GNPUP_BU3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au9[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU9_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_gnpup[] = {
	CLK_BLK_GNPUP_UID_SYSREG_GNPUP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu0[] = {
	CLK_BLK_GNPUP_UID_LH_AST_SI_LD_GNPUP_BU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpup_rdreq[] = {
	CLK_BLK_GNPUP_UID_LH_AST_SI_LD_GNPUP_RDREQ_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au17[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU17_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au16[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU16_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au19[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU19_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au18[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU18_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au13[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU13_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au12[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU12_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au15[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU15_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au14[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU14_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au31[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU31_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au0[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au30[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU30_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au1[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au11[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU11_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au2[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au10[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_npuc_setreg[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_NPUC_SETREG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_auloader[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AULOADER_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au28[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU28_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au27[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU27_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au29[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU29_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au24[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU24_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpup_npuc_done[] = {
	CLK_BLK_GNPUP_UID_LH_AST_SI_LD_GNPUP_NPUC_DONE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au23[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU23_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au26[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU26_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_gnpup[] = {
	CLK_BLK_GNPUP_UID_SLH_AXI_MI_P_GNPUP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au25[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU25_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_npupost[] = {
	CLK_BLK_GNPUP_UID_IP_NPUPOST_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au20[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU20_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_gnpup_cu_wch[] = {
	CLK_BLK_GNPUP_UID_LH_AST_SI_LD_GNPUP_CU_WCH_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au22[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU22_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au21[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_AU21_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_gnpu_sram_wrresp[] = {
	CLK_BLK_GNPUP_UID_LH_AST_MI_LD_GNPU_SRAM_WRRESP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gnss_cmu_gnss[] = {
	CLK_BLK_GNSS_UID_GNSS_CMU_GNSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_hsi0[] = {
	CLK_BLK_HSI0_UID_SLH_ASTL_SI_G_PPMU_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dp_link[] = {
	CLK_BLK_HSI0_UID_DP_LINK_IPCLKPORT_I_DP_OSC_CLK,
	CLK_BLK_HSI0_UID_DP_LINK_IPCLKPORT_I_DP_GTC_CLK,
	CLK_BLK_HSI0_UID_DP_LINK_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_hsi0_cmu_hsi0[] = {
	CLK_BLK_HSI0_UID_HSI0_CMU_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi0[] = {
	CLK_BLK_HSI0_UID_D_TZPC_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_hsi0[] = {
	CLK_BLK_HSI0_UID_SLH_AXI_MI_P_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_acel_si_d_hsi0[] = {
	CLK_BLK_HSI0_UID_SLH_ACEL_SI_D_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ld_hsi0aud[] = {
	CLK_BLK_HSI0_UID_SLH_AXI_SI_LD_HSI0AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi0_bus1[] = {
	CLK_BLK_HSI0_UID_PPMU_HSI0_BUS1_IPCLKPORT_PCLK,
	CLK_BLK_HSI0_UID_PPMU_HSI0_BUS1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_hsi0[] = {
	CLK_BLK_HSI0_UID_SYSMMU_D_HSI0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi0[] = {
	CLK_BLK_HSI0_UID_SYSREG_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usb32drd[] = {
	CLK_BLK_HSI0_UID_USB32DRD_IPCLKPORT_I_USBSUBCTL_APB_PCLK,
	CLK_BLK_HSI0_UID_USB32DRD_IPCLKPORT_I_USBDPPHY_CTRL_PCLK,
	CLK_BLK_HSI0_UID_USB32DRD_IPCLKPORT_I_USBDPPHY_TCA_APB_CLK,
	CLK_BLK_HSI0_UID_USB32DRD_IPCLKPORT_I_USBLINK_ACLK,
	CLK_BLK_HSI0_UID_USB32DRD_IPCLKPORT_I_USB32DRD_REF_CLK_40,
	CLK_BLK_HSI0_UID_USB32DRD_IPCLKPORT_I_EUSB_CTRL_PCLK,
	CLK_BLK_HSI0_UID_USB32DRD_IPCLKPORT_I_EUSB_APB_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi0[] = {
	CLK_BLK_HSI0_UID_VGEN_LITE_HSI0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_hsi0[] = {
	CLK_BLK_HSI0_UID_XIU_D_HSI0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_audhsi0[] = {
	CLK_BLK_HSI0_UID_SLH_AXI_MI_LD_AUDHSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_spc_hsi0[] = {
	CLK_BLK_HSI0_UID_SPC_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_uram[] = {
	CLK_BLK_HSI0_UID_URAM_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_p0_hsi0[] = {
	CLK_BLK_HSI0_UID_XIU_P0_HSI0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_as_apb_eusbphy_hsi0[] = {
	CLK_BLK_HSI0_UID_AS_APB_EUSBPHY_HSI0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_pcie_gen2[] = {
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_SUB_CTRL_A_G2X1_PHY_REFCLK_IN,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_PAMIR_G2X1_DWC_PCIE_CTL_INST_0_DBI_ACLK_UG,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_PHY_TOP_GEN2_X1_INST_0_PIPE_PAL_GEN2_X1_PCIE_INST_0_I_APB_PCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_PAMIR_G2X1_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_PAMIR_G2X1_DWC_PCIE_CTL_INST_0_MSTR_ACLK_UG,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_PAMIR_G2X1_DWC_PCIE_CTL_INST_0_SLV_ACLK_UG,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_PHY_TOP_GEN2_X1_INST_0_SF_PCIEPHY_X1_QCH_TM_WRAPPER_INST_0_I_APB_PCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_PHY_TOP_GEN2_X1_INST_0_PHY_UDBG_I_APB_PCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_SOC_CTRL_GEN2_IPCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi1[] = {
	CLK_BLK_HSI1_UID_D_TZPC_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi1[] = {
	CLK_BLK_HSI1_UID_GPIO_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_hsi1[] = {
	CLK_BLK_HSI1_UID_SLH_AXI_MI_P_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_si_d_hsi1[] = {
	CLK_BLK_HSI1_UID_LH_ACEL_SI_D_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	CLK_BLK_HSI1_UID_MMC_CARD_IPCLKPORT_I_ACLK,
	CLK_BLK_HSI1_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_pcie_gen3[] = {
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_PAMIR_G3X2_DWC_PCIE_CTL_INST_0_DBI_ACLK_UG,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_PAMIR_G3X2_DWC_PCIE_CTL_INST_0_MSTR_ACLK_UG,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_PAMIR_G3X2_DWC_PCIE_CTL_INST_0_SLV_ACLK_UG,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_003_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_PHY_TOP_INST_0_PHY_UDBG_I_APB_PCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_PHY_TOP_INST_0_PIPE_PAL_PCIE_INST_0_I_APB_PCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_SUB_CTRL_A_G3X2_PHY_REFCLK_IN,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_SOC_CTRL_GEN3A_IPCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN3_IPCLKPORT_PCIE_PHY_TOP_INST_0_SF_PCIEPHY_X2_QCH_TM_WRAPPER_INST_0_I_APB_PCLK,
};
enum clk_id cmucal_vclk_ip_pcie_ia_gen3[] = {
	CLK_BLK_HSI1_UID_PCIE_IA_GEN3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi1[] = {
	CLK_BLK_HSI1_UID_PPMU_HSI1_IPCLKPORT_PCLK,
	CLK_BLK_HSI1_UID_PPMU_HSI1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_hsi1[] = {
	CLK_BLK_HSI1_UID_SYSMMU_HSI1_IPCLKPORT_CLK_S1,
	CLK_BLK_HSI1_UID_SYSMMU_HSI1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi1[] = {
	CLK_BLK_HSI1_UID_SYSREG_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufs_embd[] = {
	CLK_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_ACLK,
	CLK_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_FMP_CLK,
	CLK_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_CLK_UNIPRO,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi1[] = {
	CLK_BLK_HSI1_UID_VGEN_LITE_HSI1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_hsi1[] = {
	CLK_BLK_HSI1_UID_XIU_D_HSI1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_hsi1[] = {
	CLK_BLK_HSI1_UID_XIU_P_HSI1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_hsi1_cmu_hsi1[] = {
	CLK_BLK_HSI1_UID_HSI1_CMU_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pcie_ia_gen2[] = {
	CLK_BLK_HSI1_UID_PCIE_IA_GEN2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi1ufs[] = {
	CLK_BLK_HSI1_UID_GPIO_HSI1UFS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spc_hsi1[] = {
	CLK_BLK_HSI1_UID_SPC_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_hsi1[] = {
	CLK_BLK_HSI1_UID_SLH_ASTL_SI_G_PPMU_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lme_cmu_lme[] = {
	CLK_BLK_LME_UID_LME_CMU_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d_lme[] = {
	CLK_BLK_LME_UID_LH_AXI_SI_D_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d_lme[] = {
	CLK_BLK_LME_UID_PPMU_D_LME_IPCLKPORT_PCLK,
	CLK_BLK_LME_UID_PPMU_D_LME_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_frc_mc[] = {
	CLK_BLK_LME_UID_FRC_MC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_lme[] = {
	CLK_BLK_LME_UID_AD_APB_LME_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_axi_frc_mc[] = {
	CLK_BLK_LME_UID_AD_AXI_FRC_MC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_lme[] = {
	CLK_BLK_LME_UID_SLH_AXI_MI_P_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_lme[] = {
	CLK_BLK_LME_UID_SYSREG_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_lme[] = {
	CLK_BLK_LME_UID_D_TZPC_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lme[] = {
	CLK_BLK_LME_UID_LME_IPCLKPORT_CLK,
	CLK_BLK_LME_UID_LME_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_frc_mc[] = {
	CLK_BLK_LME_UID_AD_APB_FRC_MC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_lme[] = {
	CLK_BLK_LME_UID_SYSMMU_D_LME_IPCLKPORT_CLK_S1,
	CLK_BLK_LME_UID_SYSMMU_D_LME_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d_lme[] = {
	CLK_BLK_LME_UID_XIU_D_LME_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_d0_lme[] = {
	CLK_BLK_LME_UID_QE_D0_LME_IPCLKPORT_ACLK,
	CLK_BLK_LME_UID_QE_D0_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d1_lme[] = {
	CLK_BLK_LME_UID_QE_D1_LME_IPCLKPORT_ACLK,
	CLK_BLK_LME_UID_QE_D1_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d_lme[] = {
	CLK_BLK_LME_UID_VGEN_LITE_D_LME_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_lme[] = {
	CLK_BLK_LME_UID_SLH_ASTL_SI_G_PPMU_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_m2m_cmu_m2m[] = {
	CLK_BLK_M2M_UID_M2M_CMU_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_m2m[] = {
	CLK_BLK_M2M_UID_AS_APB_M2M_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_m2m[] = {
	CLK_BLK_M2M_UID_D_TZPC_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_m2m[] = {
	CLK_BLK_M2M_UID_SLH_AXI_MI_P_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_si_d_m2m[] = {
	CLK_BLK_M2M_UID_LH_ACEL_SI_D_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_m2m[] = {
	CLK_BLK_M2M_UID_M2M_IPCLKPORT_ACLK,
	CLK_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_2X1,
	CLK_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_2X1,
	CLK_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_VOTF,
	CLK_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_VOTF,
};
enum clk_id cmucal_vclk_ip_ppmu_d_m2m[] = {
	CLK_BLK_M2M_UID_PPMU_D_M2M_IPCLKPORT_ACLK,
	CLK_BLK_M2M_UID_PPMU_D_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_astc[] = {
	CLK_BLK_M2M_UID_QE_ASTC_IPCLKPORT_PCLK,
	CLK_BLK_M2M_UID_QE_ASTC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_jpeg0[] = {
	CLK_BLK_M2M_UID_QE_JPEG0_IPCLKPORT_PCLK,
	CLK_BLK_M2M_UID_QE_JPEG0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_jpeg1[] = {
	CLK_BLK_M2M_UID_QE_JPEG1_IPCLKPORT_PCLK,
	CLK_BLK_M2M_UID_QE_JPEG1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_jsqz[] = {
	CLK_BLK_M2M_UID_QE_JSQZ_IPCLKPORT_PCLK,
	CLK_BLK_M2M_UID_QE_JSQZ_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_m2m[] = {
	CLK_BLK_M2M_UID_QE_M2M_IPCLKPORT_ACLK,
	CLK_BLK_M2M_UID_QE_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_m2m_pm[] = {
	CLK_BLK_M2M_UID_SYSMMU_D_M2M_PM_IPCLKPORT_CLK_S1,
	CLK_BLK_M2M_UID_SYSMMU_D_M2M_PM_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_m2m[] = {
	CLK_BLK_M2M_UID_SYSREG_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_m2m[] = {
	CLK_BLK_M2M_UID_XIU_D_M2M_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_astc[] = {
	CLK_BLK_M2M_UID_ASTC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_jpeg0[] = {
	CLK_BLK_M2M_UID_JPEG0_IPCLKPORT_I_SMFC_CLK,
};
enum clk_id cmucal_vclk_ip_jpeg1[] = {
	CLK_BLK_M2M_UID_JPEG1_IPCLKPORT_I_SMFC_CLK,
};
enum clk_id cmucal_vclk_ip_jsqz[] = {
	CLK_BLK_M2M_UID_JSQZ_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_m2m[] = {
	CLK_BLK_M2M_UID_VGEN_LITE_M2M_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_m2m[] = {
	CLK_BLK_M2M_UID_SLH_ASTL_SI_G_PPMU_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mcfp_cmu_mcfp[] = {
	CLK_BLK_MCFP_UID_MCFP_CMU_MCFP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mcfp[] = {
	CLK_BLK_MCFP_UID_MCFP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d2_mcfp[] = {
	CLK_BLK_MCFP_UID_LH_AXI_SI_D2_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d5_mcfp[] = {
	CLK_BLK_MCFP_UID_PPMU_D5_MCFP_IPCLKPORT_PCLK,
	CLK_BLK_MCFP_UID_PPMU_D5_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d2_mcfp[] = {
	CLK_BLK_MCFP_UID_PPMU_D2_MCFP_IPCLKPORT_PCLK,
	CLK_BLK_MCFP_UID_PPMU_D2_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_mcfp[] = {
	CLK_BLK_MCFP_UID_XIU_D1_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d5_mcfp[] = {
	CLK_BLK_MCFP_UID_SLH_AXI_SI_D5_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mcfp[] = {
	CLK_BLK_MCFP_UID_LH_AXI_SI_D1_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d4_mcfp[] = {
	CLK_BLK_MCFP_UID_SLH_AXI_SI_D4_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_mcfp[] = {
	CLK_BLK_MCFP_UID_XIU_D0_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_mcfp[] = {
	CLK_BLK_MCFP_UID_PPMU_D1_MCFP_IPCLKPORT_PCLK,
	CLK_BLK_MCFP_UID_PPMU_D1_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d3_mcfp[] = {
	CLK_BLK_MCFP_UID_SLH_AXI_SI_D3_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d4_mcfp[] = {
	CLK_BLK_MCFP_UID_PPMU_D4_MCFP_IPCLKPORT_PCLK,
	CLK_BLK_MCFP_UID_PPMU_D4_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_mcfp[] = {
	CLK_BLK_MCFP_UID_LH_AXI_SI_D0_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mcfp[] = {
	CLK_BLK_MCFP_UID_SYSREG_MCFP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_mcfp[] = {
	CLK_BLK_MCFP_UID_SYSMMU_D0_MCFP_IPCLKPORT_CLK_S1,
	CLK_BLK_MCFP_UID_SYSMMU_D0_MCFP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_mcfp[] = {
	CLK_BLK_MCFP_UID_SYSMMU_D2_MCFP_IPCLKPORT_CLK_S1,
	CLK_BLK_MCFP_UID_SYSMMU_D2_MCFP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcfp[] = {
	CLK_BLK_MCFP_UID_D_TZPC_MCFP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_mcfp[] = {
	CLK_BLK_MCFP_UID_SYSMMU_D1_MCFP_IPCLKPORT_CLK_S1,
	CLK_BLK_MCFP_UID_SYSMMU_D1_MCFP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d4_mcfp[] = {
	CLK_BLK_MCFP_UID_SYSMMU_D4_MCFP_IPCLKPORT_CLK_S1,
	CLK_BLK_MCFP_UID_SYSMMU_D4_MCFP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_d3_mcfp[] = {
	CLK_BLK_MCFP_UID_PPMU_D3_MCFP_IPCLKPORT_PCLK,
	CLK_BLK_MCFP_UID_PPMU_D3_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d3_mcfp[] = {
	CLK_BLK_MCFP_UID_SYSMMU_D3_MCFP_IPCLKPORT_CLK_S1,
	CLK_BLK_MCFP_UID_SYSMMU_D3_MCFP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcfp[] = {
	CLK_BLK_MCFP_UID_AD_APB_MCFP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_mcfp[] = {
	CLK_BLK_MCFP_UID_PPMU_D0_MCFP_IPCLKPORT_PCLK,
	CLK_BLK_MCFP_UID_PPMU_D0_MCFP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d5_mcfp[] = {
	CLK_BLK_MCFP_UID_SYSMMU_D5_MCFP_IPCLKPORT_CLK_S1,
	CLK_BLK_MCFP_UID_SYSMMU_D5_MCFP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mcfp[] = {
	CLK_BLK_MCFP_UID_SLH_AXI_MI_P_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf_rgbpmcfp[] = {
	CLK_BLK_MCFP_UID_LH_AST_MI_OTF_RGBPMCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf0_mcfpyuvp[] = {
	CLK_BLK_MCFP_UID_LH_AST_SI_OTF0_MCFPYUVP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf_yuvpmcfp[] = {
	CLK_BLK_MCFP_UID_LH_AST_MI_OTF_YUVPMCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d0_mcfp[] = {
	CLK_BLK_MCFP_UID_VGEN_LITE_D0_MCFP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d1_mcfp[] = {
	CLK_BLK_MCFP_UID_VGEN_LITE_D1_MCFP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_mcfp[] = {
	CLK_BLK_MCFP_UID_SLH_ASTL_SI_G_PPMU_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g_ppmu_mcfp[] = {
	CLK_BLK_MCFP_UID_SIU_G_PPMU_MCFP_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_mcsc_cmu_mcsc[] = {
	CLK_BLK_MCSC_UID_MCSC_CMU_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_gdc[] = {
	CLK_BLK_MCSC_UID_AD_APB_GDC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcsc[] = {
	CLK_BLK_MCSC_UID_AD_APB_MCSC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcsc[] = {
	CLK_BLK_MCSC_UID_D_TZPC_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gdc[] = {
	CLK_BLK_MCSC_UID_GDC_IPCLKPORT_C2CLK_M,
	CLK_BLK_MCSC_UID_GDC_IPCLKPORT_C2CLK_S,
	CLK_BLK_MCSC_UID_GDC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf_drcpmcsc[] = {
	CLK_BLK_MCSC_UID_LH_AST_MI_OTF_DRCPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mcsc[] = {
	CLK_BLK_MCSC_UID_SLH_AXI_MI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_si_d0_mcsc[] = {
	CLK_BLK_MCSC_UID_LH_ACEL_SI_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mcsc[] = {
	CLK_BLK_MCSC_UID_LH_AXI_SI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d2_mcsc[] = {
	CLK_BLK_MCSC_UID_LH_AXI_SI_D2_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mcsc[] = {
	CLK_BLK_MCSC_UID_MCSC_IPCLKPORT_C2R_CLK,
	CLK_BLK_MCSC_UID_MCSC_IPCLKPORT_CLK,
	CLK_BLK_MCSC_UID_MCSC_IPCLKPORT_C2W_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_mcsc[] = {
	CLK_BLK_MCSC_UID_PPMU_D0_MCSC_IPCLKPORT_ACLK,
	CLK_BLK_MCSC_UID_PPMU_D0_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_mcsc[] = {
	CLK_BLK_MCSC_UID_PPMU_D1_MCSC_IPCLKPORT_ACLK,
	CLK_BLK_MCSC_UID_PPMU_D1_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d2_mcsc[] = {
	CLK_BLK_MCSC_UID_PPMU_D2_MCSC_IPCLKPORT_ACLK,
	CLK_BLK_MCSC_UID_PPMU_D2_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_mcsc[] = {
	CLK_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S1,
	CLK_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_mcsc[] = {
	CLK_BLK_MCSC_UID_SYSMMU_D2_MCSC_IPCLKPORT_CLK_S1,
	CLK_BLK_MCSC_UID_SYSMMU_D2_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_mcsc[] = {
	CLK_BLK_MCSC_UID_SYSREG_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_mcsc[] = {
	CLK_BLK_MCSC_UID_XIU_D0_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_mcsc[] = {
	CLK_BLK_MCSC_UID_XIU_D1_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d2_mcsc[] = {
	CLK_BLK_MCSC_UID_XIU_D2_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_mcsc[] = {
	CLK_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S1,
	CLK_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d0_mcsc[] = {
	CLK_BLK_MCSC_UID_VGEN_LITE_D0_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d1_mcsc[] = {
	CLK_BLK_MCSC_UID_VGEN_LITE_D1_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_mcsc[] = {
	CLK_BLK_MCSC_UID_SLH_ASTL_SI_G_PPMU_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g_ppmu_mcsc[] = {
	CLK_BLK_MCSC_UID_SIU_G_PPMU_MCSC_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_ig_ppmu_d0_mcsc[] = {
	CLK_BLK_MCSC_UID_SLH_ASTL_MI_IG_PPMU_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_ig_ppmu_d0_mcsc[] = {
	CLK_BLK_MCSC_UID_SLH_ASTL_SI_IG_PPMU_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mfc0_cmu_mfc0[] = {
	CLK_BLK_MFC0_UID_MFC0_CMU_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mfc0[] = {
	CLK_BLK_MFC0_UID_XIU_D_MFC0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_wfd[] = {
	CLK_BLK_MFC0_UID_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc0d0[] = {
	CLK_BLK_MFC0_UID_SYSMMU_MFC0D0_IPCLKPORT_CLK_S1,
	CLK_BLK_MFC0_UID_SYSMMU_MFC0D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_wfd[] = {
	CLK_BLK_MFC0_UID_PPMU_WFD_IPCLKPORT_PCLK,
	CLK_BLK_MFC0_UID_PPMU_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc0d1[] = {
	CLK_BLK_MFC0_UID_PPMU_MFC0D1_IPCLKPORT_ACLK,
	CLK_BLK_MFC0_UID_PPMU_MFC0D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc0d0[] = {
	CLK_BLK_MFC0_UID_PPMU_MFC0D0_IPCLKPORT_ACLK,
	CLK_BLK_MFC0_UID_PPMU_MFC0D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mfc0[] = {
	CLK_BLK_MFC0_UID_MFC0_IPCLKPORT_ACLK,
	CLK_BLK_MFC0_UID_MFC0_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mfc0[] = {
	CLK_BLK_MFC0_UID_LH_ATB_MFC0_IPCLKPORT_I_CLK_SI,
	CLK_BLK_MFC0_UID_LH_ATB_MFC0_IPCLKPORT_I_CLK_MI,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mfc0[] = {
	CLK_BLK_MFC0_UID_LH_AXI_SI_D1_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_mfc0[] = {
	CLK_BLK_MFC0_UID_LH_AXI_SI_D0_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf2_mfc0mfc1[] = {
	CLK_BLK_MFC0_UID_LH_AST_SI_OTF2_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf1_mfc0mfc1[] = {
	CLK_BLK_MFC0_UID_LH_AST_SI_OTF1_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf0_mfc0mfc1[] = {
	CLK_BLK_MFC0_UID_LH_AST_SI_OTF0_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mfc0[] = {
	CLK_BLK_MFC0_UID_SLH_AXI_MI_P_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf3_mfc1mfc0[] = {
	CLK_BLK_MFC0_UID_LH_AST_MI_OTF3_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf2_mfc1mfc0[] = {
	CLK_BLK_MFC0_UID_LH_AST_MI_OTF2_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf1_mfc1mfc0[] = {
	CLK_BLK_MFC0_UID_LH_AST_MI_OTF1_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf0_mfc1mfc0[] = {
	CLK_BLK_MFC0_UID_LH_AST_MI_OTF0_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfc0[] = {
	CLK_BLK_MFC0_UID_D_TZPC_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_axi_wfd[] = {
	CLK_BLK_MFC0_UID_AS_AXI_WFD_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ads_apb_mfc0mfc1[] = {
	CLK_BLK_MFC0_UID_ADS_APB_MFC0MFC1_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc0[] = {
	CLK_BLK_MFC0_UID_AS_APB_MFC0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_as_apb_wfd_ns[] = {
	CLK_BLK_MFC0_UID_AS_APB_WFD_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc0[] = {
	CLK_BLK_MFC0_UID_SYSREG_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf3_mfc0mfc1[] = {
	CLK_BLK_MFC0_UID_LH_AST_SI_OTF3_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc0d1[] = {
	CLK_BLK_MFC0_UID_SYSMMU_MFC0D1_IPCLKPORT_CLK_S1,
	CLK_BLK_MFC0_UID_SYSMMU_MFC0D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_mfc0[] = {
	CLK_BLK_MFC0_UID_SLH_ASTL_SI_G_PPMU_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g_ppmu_mfc0[] = {
	CLK_BLK_MFC0_UID_SIU_G_PPMU_MFC0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_mfc0[] = {
	CLK_BLK_MFC0_UID_VGEN_LITE_MFC0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mfc1_cmu_mfc1[] = {
	CLK_BLK_MFC1_UID_MFC1_CMU_MFC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc1[] = {
	CLK_BLK_MFC1_UID_SYSREG_MFC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc1d0[] = {
	CLK_BLK_MFC1_UID_SYSMMU_MFC1D0_IPCLKPORT_CLK_S1,
	CLK_BLK_MFC1_UID_SYSMMU_MFC1D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf0_mfc0mfc1[] = {
	CLK_BLK_MFC1_UID_LH_AST_MI_OTF0_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfc1[] = {
	CLK_BLK_MFC1_UID_D_TZPC_MFC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc1[] = {
	CLK_BLK_MFC1_UID_AS_APB_MFC1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_adm_apb_mfc0mfc1[] = {
	CLK_BLK_MFC1_UID_ADM_APB_MFC0MFC1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf1_mfc0mfc1[] = {
	CLK_BLK_MFC1_UID_LH_AST_MI_OTF1_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf2_mfc0mfc1[] = {
	CLK_BLK_MFC1_UID_LH_AST_MI_OTF2_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf3_mfc0mfc1[] = {
	CLK_BLK_MFC1_UID_LH_AST_MI_OTF3_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mfc1[] = {
	CLK_BLK_MFC1_UID_SLH_AXI_MI_P_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf0_mfc1mfc0[] = {
	CLK_BLK_MFC1_UID_LH_AST_SI_OTF0_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf1_mfc1mfc0[] = {
	CLK_BLK_MFC1_UID_LH_AST_SI_OTF1_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf2_mfc1mfc0[] = {
	CLK_BLK_MFC1_UID_LH_AST_SI_OTF2_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf3_mfc1mfc0[] = {
	CLK_BLK_MFC1_UID_LH_AST_SI_OTF3_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_mfc1[] = {
	CLK_BLK_MFC1_UID_LH_AXI_SI_D0_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_mfc1[] = {
	CLK_BLK_MFC1_UID_LH_AXI_SI_D1_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mfc1[] = {
	CLK_BLK_MFC1_UID_MFC1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc1d0[] = {
	CLK_BLK_MFC1_UID_PPMU_MFC1D0_IPCLKPORT_ACLK,
	CLK_BLK_MFC1_UID_PPMU_MFC1D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc1d1[] = {
	CLK_BLK_MFC1_UID_PPMU_MFC1D1_IPCLKPORT_PCLK,
	CLK_BLK_MFC1_UID_PPMU_MFC1D1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc1d1[] = {
	CLK_BLK_MFC1_UID_SYSMMU_MFC1D1_IPCLKPORT_CLK_S1,
	CLK_BLK_MFC1_UID_SYSMMU_MFC1D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_vgen_mfc1[] = {
	CLK_BLK_MFC1_UID_VGEN_MFC1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_mfc1[] = {
	CLK_BLK_MFC1_UID_SLH_ASTL_SI_G_PPMU_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g_ppmu_mfc1[] = {
	CLK_BLK_MFC1_UID_SIU_G_PPMU_MFC1_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_mif_cmu_mif[] = {
	CLK_BLK_MIF_UID_MIF_CMU_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_ddrphy[] = {
	CLK_BLK_MIF_UID_APBBR_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmc[] = {
	CLK_BLK_MIF_UID_APBBR_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ddrphy[] = {
	CLK_BLK_MIF_UID_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	CLK_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif[] = {
	CLK_BLK_MIF_UID_D_TZPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_mif[] = {
	CLK_BLK_MIF_UID_SLH_AXI_MI_P_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppc_debug[] = {
	CLK_BLK_MIF_UID_QCH_ADAPTER_PPC_DEBUG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif[] = {
	CLK_BLK_MIF_UID_SYSREG_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_g_dmc[] = {
	CLK_BLK_MIF_UID_LH_AST_SI_G_DMC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_spc_mif[] = {
	CLK_BLK_MIF_UID_SPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_private_mif[] = {
	CLK_BLK_MIF_UID_SYSREG_PRIVATE_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_mif[] = {
	CLK_BLK_MIF_UID_SLH_ASTL_SI_G_PPMU_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_ddd_mif[] = {
	CLK_BLK_MIF_UID_BUSIF_DDD_MIF_IPCLKPORT_PCLK,
	CLK_BLK_MIF_UID_BUSIF_DDD_MIF_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_ddd_mif[] = {
	CLK_BLK_MIF_UID_DDD_MIF_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_ppc_debug[] = {
	CLK_BLK_MIF_UID_PPC_DEBUG_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_nocl0_cmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_NOCL0_CMU_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cci[] = {
	CLK_BLK_NOCL0_UID_CCI_IPCLKPORT_CLK,
	CLK_BLK_NOCL0_UID_CCI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_siu_g0_ppmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_SIU_G0_PPMU_NOCL0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_g1_ppmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_SIU_G1_PPMU_NOCL0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_apb_async_sysmmu_s2_g3d[] = {
	CLK_BLK_NOCL0_UID_APB_ASYNC_SYSMMU_S2_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_axi_asb_cssys[] = {
	CLK_BLK_NOCL0_UID_AXI_ASB_CSSYS_IPCLKPORT_ACLKM,
	CLK_BLK_NOCL0_UID_AXI_ASB_CSSYS_IPCLKPORT_ACLKS,
	CLK_BLK_NOCL0_UID_AXI_ASB_CSSYS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_cp[] = {
	CLK_BLK_NOCL0_UID_BAAW_CP_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bdu[] = {
	CLK_BLK_NOCL0_UID_BDU_IPCLKPORT_I_CLK,
	CLK_BLK_NOCL0_UID_BDU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_nocl0[] = {
	CLK_BLK_NOCL0_UID_D_TZPC_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d2_modem[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_MI_D2_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_chi_mi_d0_cluster0[] = {
	CLK_BLK_NOCL0_UID_LH_CHI_MI_D0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_d0_g3d[] = {
	CLK_BLK_NOCL0_UID_LH_ACEL_MI_D0_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_chi_mi_d1_cluster0[] = {
	CLK_BLK_NOCL0_UID_LH_CHI_MI_D1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_d1_g3d[] = {
	CLK_BLK_NOCL0_UID_LH_ACEL_MI_D1_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_d2_g3d[] = {
	CLK_BLK_NOCL0_UID_LH_ACEL_MI_D2_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_d3_g3d[] = {
	CLK_BLK_NOCL0_UID_LH_ACEL_MI_D3_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d0_modem[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_MI_D0_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d1_modem[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_MI_D1_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_qdi_mi_d_aud[] = {
	CLK_BLK_NOCL0_UID_LH_QDI_MI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_g_cssys[] = {
	CLK_BLK_NOCL0_UID_LH_AXI_MI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_si_t_bdu[] = {
	CLK_BLK_NOCL0_UID_LH_ATB_SI_T_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_apm[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_aud[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_cpucl0[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_g3d[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_modem[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_perisgic[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_PERISGIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace2axi_0[] = {
	CLK_BLK_NOCL0_UID_MPACE2AXI_0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mpace2axi_1[] = {
	CLK_BLK_NOCL0_UID_MPACE2AXI_1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d0_mif[] = {
	CLK_BLK_NOCL0_UID_MPACE_ASB_D0_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d1_mif[] = {
	CLK_BLK_NOCL0_UID_MPACE_ASB_D1_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d2_mif[] = {
	CLK_BLK_NOCL0_UID_MPACE_ASB_D2_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d3_mif[] = {
	CLK_BLK_NOCL0_UID_MPACE_ASB_D3_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_debug_cci[] = {
	CLK_BLK_NOCL0_UID_PPC_DEBUG_CCI_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPC_DEBUG_CCI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl0_0[] = {
	CLK_BLK_NOCL0_UID_PPMU_CPUCL0_0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl0_1[] = {
	CLK_BLK_NOCL0_UID_PPMU_CPUCL0_1_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d0[] = {
	CLK_BLK_NOCL0_UID_PPMU_G3D0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_G3D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d1[] = {
	CLK_BLK_NOCL0_UID_PPMU_G3D1_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_G3D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d2[] = {
	CLK_BLK_NOCL0_UID_PPMU_G3D2_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_G3D2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d3[] = {
	CLK_BLK_NOCL0_UID_PPMU_G3D3_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_G3D3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif0[] = {
	CLK_BLK_NOCL0_UID_PPMU_MIF0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_MIF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif1[] = {
	CLK_BLK_NOCL0_UID_PPMU_MIF1_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_MIF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif2[] = {
	CLK_BLK_NOCL0_UID_PPMU_MIF2_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_MIF2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif3[] = {
	CLK_BLK_NOCL0_UID_PPMU_MIF3_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_MIF3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_g3d[] = {
	CLK_BLK_NOCL0_UID_SYSMMU_S2_G3D_IPCLKPORT_CLK_S2_D0,
	CLK_BLK_NOCL0_UID_SYSMMU_S2_G3D_IPCLKPORT_CLK_S2_D1,
	CLK_BLK_NOCL0_UID_SYSMMU_S2_G3D_IPCLKPORT_CLK_S2_D2,
	CLK_BLK_NOCL0_UID_SYSMMU_S2_G3D_IPCLKPORT_CLK_S2_D3,
	CLK_BLK_NOCL0_UID_SYSMMU_S2_G3D_IPCLKPORT_CLK_MPTW,
};
enum clk_id cmucal_vclk_ip_sysmmu_modem[] = {
	CLK_BLK_NOCL0_UID_SYSMMU_MODEM_IPCLKPORT_CLK_S2,
	CLK_BLK_NOCL0_UID_SYSMMU_MODEM_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysreg_nocl0[] = {
	CLK_BLK_NOCL0_UID_SYSREG_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_nocl0[] = {
	CLK_BLK_NOCL0_UID_TREX_D_NOCL0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_TREX_D_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p_nocl0[] = {
	CLK_BLK_NOCL0_UID_TREX_P_NOCL0_IPCLKPORT_PCLK,
	CLK_BLK_NOCL0_UID_TREX_P_NOCL0_IPCLKPORT_ACLK_NOCL0,
	CLK_BLK_NOCL0_UID_TREX_P_NOCL0_IPCLKPORT_PCLK_NOCL0,
};
enum clk_id cmucal_vclk_ip_vgen_lite_modem[] = {
	CLK_BLK_NOCL0_UID_VGEN_LITE_MODEM_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_asyncsfr_wr_smc[] = {
	CLK_BLK_NOCL0_UID_ASYNCSFR_WR_SMC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mif0[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mif1[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mif2[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_MIF2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mif3[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_MIF3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_apm[] = {
	CLK_BLK_NOCL0_UID_SYSMMU_S2_APM_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_apm[] = {
	CLK_BLK_NOCL0_UID_LH_AXI_MI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mcw[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_SI_P_MCW_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_cluster0_nocl0[] = {
	CLK_BLK_NOCL0_UID_SLH_AXI_MI_P_CLUSTER0_NOCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cacheaid_nocl0[] = {
	CLK_BLK_NOCL0_UID_CACHEAID_NOCL0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_CACHEAID_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_apm[] = {
	CLK_BLK_NOCL0_UID_PPMU_APM_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g0_dmc[] = {
	CLK_BLK_NOCL0_UID_LH_AST_MI_G0_DMC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g1_dmc[] = {
	CLK_BLK_NOCL0_UID_LH_AST_MI_G1_DMC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g2_dmc[] = {
	CLK_BLK_NOCL0_UID_LH_AST_MI_G2_DMC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g3_dmc[] = {
	CLK_BLK_NOCL0_UID_LH_AST_MI_G3_DMC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g_nocl1a[] = {
	CLK_BLK_NOCL0_UID_LH_AST_MI_G_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g_nocl1b[] = {
	CLK_BLK_NOCL0_UID_LH_AST_MI_G_NOCL1B_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_g_nocl2a[] = {
	CLK_BLK_NOCL0_UID_LH_AST_MI_G_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_d0_mif[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_D0_MIF_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_D0_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_irps0[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_irps1[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS1_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_irps2[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS2_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_irps3[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS3_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_IRPS3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_nocl0[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_NOCL0_IPCLKPORT_CLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_sci[] = {
	CLK_BLK_NOCL0_UID_PPC_SCI_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPC_SCI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spc_nocl0[] = {
	CLK_BLK_NOCL0_UID_SPC_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_private_nocl0[] = {
	CLK_BLK_NOCL0_UID_SYSREG_PRIVATE_NOCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_d0_cpucl0[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_D0_CPUCL0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_D0_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_d0_g3d[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_D0_G3D_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_D0_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_d1_cpucl0[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_D1_CPUCL0_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_D1_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_aud[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_modem[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_nocl1a[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_nocl1b[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_NOCL1B_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g6_ppmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_SIU_G6_PPMU_NOCL0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_g2_ppmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_SIU_G2_PPMU_NOCL0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_g3_ppmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_SIU_G3_PPMU_NOCL0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_g4_ppmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_SIU_G4_PPMU_NOCL0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_g5_ppmu_nocl0[] = {
	CLK_BLK_NOCL0_UID_SIU_G5_PPMU_NOCL0_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_apb_async_etr[] = {
	CLK_BLK_NOCL0_UID_APB_ASYNC_ETR_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_etr_64_nocl0[] = {
	CLK_BLK_NOCL0_UID_ETR_64_NOCL0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif0[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif1[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif2[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_MIF2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif3[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_G_PPMU_MIF3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_sync_gen[] = {
	CLK_BLK_NOCL0_UID_PPMU_SYNC_GEN_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_PPMU_SYNC_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pbha_gen_d0_modem[] = {
	CLK_BLK_NOCL0_UID_PBHA_GEN_D0_MODEM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_pbha_gen_d1_modem[] = {
	CLK_BLK_NOCL0_UID_PBHA_GEN_D1_MODEM_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_d1_mif[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_D1_MIF_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_D1_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_d2_mif[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_D2_MIF_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_D2_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wow_dvfs_d3_mif[] = {
	CLK_BLK_NOCL0_UID_WOW_DVFS_D3_MIF_IPCLKPORT_ACLK,
	CLK_BLK_NOCL0_UID_WOW_DVFS_D3_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_d0_g3d[] = {
	CLK_BLK_NOCL0_UID_VGEN_D0_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_d1_g3d[] = {
	CLK_BLK_NOCL0_UID_VGEN_D1_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_d2_g3d[] = {
	CLK_BLK_NOCL0_UID_VGEN_D2_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_d3_g3d[] = {
	CLK_BLK_NOCL0_UID_VGEN_D3_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_ig_ppmu_p_cluster0[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_SI_IG_PPMU_P_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_ig_ppmu_p_cluster0[] = {
	CLK_BLK_NOCL0_UID_SLH_ASTL_MI_IG_PPMU_P_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apb_async_vgen_d0_g3d[] = {
	CLK_BLK_NOCL0_UID_APB_ASYNC_VGEN_D0_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_apb_async_vgen_d1_g3d[] = {
	CLK_BLK_NOCL0_UID_APB_ASYNC_VGEN_D1_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_apb_async_vgen_d2_g3d[] = {
	CLK_BLK_NOCL0_UID_APB_ASYNC_VGEN_D2_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_apb_async_vgen_d3_g3d[] = {
	CLK_BLK_NOCL0_UID_APB_ASYNC_VGEN_D3_G3D_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl0[] = {
	CLK_BLK_NOCL0_UID_SLH_AST_SI_IG_DEBUG_MUX_NOCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl0[] = {
	CLK_BLK_NOCL0_UID_SLH_AST_MI_IG_DEBUG_MUX_NOCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_nocl1a_cmu_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_NOCL1A_CMU_NOCL1A_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_ssp[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SYSREG_NOCL1A_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_TREX_D_NOCL1A_IPCLKPORT_PCLK,
	CLK_BLK_NOCL1A_UID_TREX_D_NOCL1A_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_hsi1[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mfc1[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_mfc1[] = {
	CLK_BLK_NOCL1A_UID_LH_AXI_MI_D1_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_g_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_LH_AST_SI_G_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_mfc0[] = {
	CLK_BLK_NOCL1A_UID_LH_AXI_MI_D1_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mfc0[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_nocif_cmutopc[] = {
	CLK_BLK_NOCL1A_UID_NOCIF_CMUTOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_d_m2m[] = {
	CLK_BLK_NOCL1A_UID_LH_ACEL_MI_D_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_D_TZPC_NOCL1A_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_lme[] = {
	CLK_BLK_NOCL1A_UID_LH_AXI_MI_D_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_m2m[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_p_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_TREX_P_NOCL1A_IPCLKPORT_PCLK,
	CLK_BLK_NOCL1A_UID_TREX_P_NOCL1A_IPCLKPORT_PCLK_NOCL1A,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_mfc1[] = {
	CLK_BLK_NOCL1A_UID_LH_AXI_MI_D0_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_lme[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_mfc0[] = {
	CLK_BLK_NOCL1A_UID_LH_AXI_MI_D0_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_d_hsi1[] = {
	CLK_BLK_NOCL1A_UID_LH_ACEL_MI_D_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_acel_mi_d_ssp[] = {
	CLK_BLK_NOCL1A_UID_SLH_ACEL_MI_D_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_hsi1[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_G_PPMU_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_lme[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_G_PPMU_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_m2m[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_G_PPMU_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mfc0[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_G_PPMU_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mfc1[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_G_PPMU_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_SI_G_PPMU_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_nocl2a[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_G_PPMU_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_ssp[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_G_PPMU_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_2x1_p0_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SIU_2X1_P0_NOCL1A_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_4x1_p0_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SIU_4X1_P0_NOCL1A_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_8x1_p0_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SIU_8X1_P0_NOCL1A_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_peric0[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_PERIC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_peric2[] = {
	CLK_BLK_NOCL1A_UID_SLH_AXI_SI_P_PERIC2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_ig_ppmu_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_MI_IG_PPMU_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_ig_ppmu_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SLH_ASTL_SI_IG_PPMU_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SLH_AST_MI_IG_DEBUG_MUX_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl1a[] = {
	CLK_BLK_NOCL1A_UID_SLH_AST_SI_IG_DEBUG_MUX_NOCL1A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_nocl1b_cmu_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_NOCL1B_CMU_NOCL1B_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_vts[] = {
	CLK_BLK_NOCL1B_UID_BAAW_P_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_D_TZPC_NOCL1B_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_acel_mi_d_hsi0[] = {
	CLK_BLK_NOCL1B_UID_SLH_ACEL_MI_D_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d0_dpuf0[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_MI_D0_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_dpuf0[] = {
	CLK_BLK_NOCL1B_UID_LH_AXI_MI_D1_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_dpub[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_DPUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_dpuf0[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_dpuf1[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_hsi0[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_vts[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_g_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_LH_AST_SI_G_NOCL1B_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_pdma[] = {
	CLK_BLK_NOCL1B_UID_QE_PDMA_IPCLKPORT_PCLK,
	CLK_BLK_NOCL1B_UID_QE_PDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_spdma[] = {
	CLK_BLK_NOCL1B_UID_QE_SPDMA_IPCLKPORT_PCLK,
	CLK_BLK_NOCL1B_UID_QE_SPDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_SYSREG_NOCL1B_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_TREX_D_NOCL1B_IPCLKPORT_ACLK,
	CLK_BLK_NOCL1B_UID_TREX_D_NOCL1B_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_TREX_P_NOCL1B_IPCLKPORT_PCLK_NOCL1B,
	CLK_BLK_NOCL1B_UID_TREX_P_NOCL1B_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_rb_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_TREX_RB_NOCL1B_IPCLKPORT_PCLK,
	CLK_BLK_NOCL1B_UID_TREX_RB_NOCL1B_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_chub[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_CHUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_gnss[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_chub[] = {
	CLK_BLK_NOCL1B_UID_BAAW_P_CHUB_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_gnss[] = {
	CLK_BLK_NOCL1B_UID_BAAW_P_GNSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_ufd[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_UFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_dnc[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_dsp0[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_DSP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_dsp1[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_DSP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_peris[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_PERIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_sdma[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_SDMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d_tt[] = {
	CLK_BLK_NOCL1B_UID_PPMU_D_TT_IPCLKPORT_PCLK,
	CLK_BLK_NOCL1B_UID_PPMU_D_TT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dit[] = {
	CLK_BLK_NOCL1B_UID_PPMU_DIT_IPCLKPORT_PCLK,
	CLK_BLK_NOCL1B_UID_PPMU_DIT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_dnc[] = {
	CLK_BLK_NOCL1B_UID_BAAW_P_DNC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_hsi0[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_G_PPMU_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_p0_trex_xiu_dit[] = {
	CLK_BLK_NOCL1B_UID_SIU_P0_TREX_XIU_DIT_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_p1_hsi_dpuf_dnc_gnss[] = {
	CLK_BLK_NOCL1B_UID_SIU_P1_HSI_DPUF_DNC_GNSS_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_dpuf0[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_G_PPMU_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_dpuf1[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_G_PPMU_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_dnc[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_G_PPMU_DNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_SI_G_PPMU_NOCL1B_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_gnss[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_G_PPMU_GNSS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_peric1[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_SI_P_PERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dit[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_PDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_spdma[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_SPDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_dit[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_SYSMMU_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_tt[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_SYSMMU_TT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_tt_ns[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_SYSMMU_TT_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_tt_s[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_SYSMMU_TT_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_ufd[] = {
	CLK_BLK_NOCL1B_UID_SLH_AXI_MI_D_UFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma[] = {
	CLK_BLK_NOCL1B_UID_PDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_spdma[] = {
	CLK_BLK_NOCL1B_UID_SPDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_dit[] = {
	CLK_BLK_NOCL1B_UID_DIT_IPCLKPORT_ICLKL2A,
};
enum clk_id cmucal_vclk_ip_xiu_d_tt[] = {
	CLK_BLK_NOCL1B_UID_XIU_D_TT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_tt[] = {
	CLK_BLK_NOCL1B_UID_SYSMMU_TT_IPCLKPORT_CLK_S1,
	CLK_BLK_NOCL1B_UID_SYSMMU_TT_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_dit[] = {
	CLK_BLK_NOCL1B_UID_SYSMMU_S2_DIT_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_vgen_lite_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_VGEN_LITE_NOCL1B_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_spdma[] = {
	CLK_BLK_NOCL1B_UID_VGEN_SPDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_pdma[] = {
	CLK_BLK_NOCL1B_UID_VGEN_PDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_tt[] = {
	CLK_BLK_NOCL1B_UID_AD_AXI_TT_IPCLKPORT_ACLKS,
	CLK_BLK_NOCL1B_UID_AD_AXI_TT_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_id_dit[] = {
	CLK_BLK_NOCL1B_UID_LH_ACEL_MI_ID_DIT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_si_id_dit[] = {
	CLK_BLK_NOCL1B_UID_LH_ACEL_SI_ID_DIT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_lite_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_VGEN_LITE_NOCL1B_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_pdma[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_VGEN_PDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_spdma[] = {
	CLK_BLK_NOCL1B_UID_AD_APB_VGEN_SPDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_ig_ppmu_trexp[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_SI_IG_PPMU_TREXP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_ig_ppmu_xiu_d_tt[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_SI_IG_PPMU_XIU_D_TT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_ig_ppmu_dit[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_SI_IG_PPMU_DIT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_ig_ppmu_trexp[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_IG_PPMU_TREXP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_ig_ppmu_xiu_d_tt[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_IG_PPMU_XIU_D_TT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_ig_ppmu_dit[] = {
	CLK_BLK_NOCL1B_UID_SLH_ASTL_MI_IG_PPMU_DIT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_SLH_AST_MI_IG_DEBUG_MUX_NOCL1B_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl1b[] = {
	CLK_BLK_NOCL1B_UID_SLH_AST_SI_IG_DEBUG_MUX_NOCL1B_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_nocl2a_cmu_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_NOCL2A_CMU_NOCL2A_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d2_mcsc[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D2_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SYSREG_NOCL2A_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_TREX_D_NOCL2A_IPCLKPORT_PCLK,
	CLK_BLK_NOCL2A_UID_TREX_D_NOCL2A_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_cstat[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_CSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_cstat[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D_CSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_mcfp[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D1_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_csis[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mcfp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_csis[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d4_mcfp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_MI_D4_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_rgbp[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D0_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_byrp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_BYRP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_acel_mi_d0_mcsc[] = {
	CLK_BLK_NOCL2A_UID_LH_ACEL_MI_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_p_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_TREX_P_NOCL2A_IPCLKPORT_PCLK,
	CLK_BLK_NOCL2A_UID_TREX_P_NOCL2A_IPCLKPORT_PCLK_NOCL2A,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_mcfp[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D0_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_byrp[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D_BYRP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_yuvp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_YUVP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d3_csis[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D3_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d3_mcfp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_MI_D3_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_g_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_LH_AST_SI_G_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_mcsc[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_mcsc[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d0_csis[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d_yuvp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_MI_D_YUVP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_D_TZPC_NOCL2A_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_drcp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_DRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_p_rgbp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_SI_P_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d_drcp[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D_DRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d2_csis[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D2_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_d5_mcfp[] = {
	CLK_BLK_NOCL2A_UID_SLH_AXI_MI_D5_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d2_mcfp[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D2_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d4_csis[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D4_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_d1_rgbp[] = {
	CLK_BLK_NOCL2A_UID_LH_AXI_MI_D1_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_byrp[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_BYRP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_csis[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_cstat[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_CSTAT_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_drcp[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_DRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mcfp[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_MCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_mcsc[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_rgbp[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_g_ppmu_yuvp[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_G_PPMU_YUVP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_SI_G_PPMU_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_2x1_p0_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SIU_2X1_P0_NOCL2A_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_4x1_p0_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SIU_4X1_P0_NOCL2A_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_siu_8x1_p1_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SIU_8X1_P1_NOCL2A_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_mi_ig_ppmu_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_MI_IG_PPMU_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_ig_ppmu_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SLH_ASTL_SI_IG_PPMU_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SLH_AST_MI_IG_DEBUG_MUX_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl2a[] = {
	CLK_BLK_NOCL2A_UID_SLH_AST_SI_IG_DEBUG_MUX_NOCL2A_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_peric0_cmu_peric0[] = {
	CLK_BLK_PERIC0_UID_PERIC0_CMU_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peric0[] = {
	CLK_BLK_PERIC0_UID_SYSREG_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peric0[] = {
	CLK_BLK_PERIC0_UID_GPIO_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_peric0[] = {
	CLK_BLK_PERIC0_UID_SLH_AXI_MI_P_PERIC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric0[] = {
	CLK_BLK_PERIC0_UID_D_TZPC_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi04_usi[] = {
	CLK_BLK_PERIC0_UID_USI04_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC0_UID_USI04_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi04_i2c[] = {
	CLK_BLK_PERIC0_UID_USI04_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC0_UID_USI04_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_peric1_cmu_peric1[] = {
	CLK_BLK_PERIC1_UID_PERIC1_CMU_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peric1[] = {
	CLK_BLK_PERIC1_UID_GPIO_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric1[] = {
	CLK_BLK_PERIC1_UID_D_TZPC_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_csisperic1[] = {
	CLK_BLK_PERIC1_UID_SLH_AXI_MI_P_CSISPERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_peric1[] = {
	CLK_BLK_PERIC1_UID_SLH_AXI_MI_P_PERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peric1[] = {
	CLK_BLK_PERIC1_UID_SYSREG_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_peric1[] = {
	CLK_BLK_PERIC1_UID_XIU_P_PERIC1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_i3c00[] = {
	CLK_BLK_PERIC1_UID_I3C00_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC1_UID_I3C00_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c01[] = {
	CLK_BLK_PERIC1_UID_I3C01_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC1_UID_I3C01_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c02[] = {
	CLK_BLK_PERIC1_UID_I3C02_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC1_UID_I3C02_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_bt_uart[] = {
	CLK_BLK_PERIC1_UID_BT_UART_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_BT_UART_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi07_usi[] = {
	CLK_BLK_PERIC1_UID_USI07_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI07_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi07_spi_i2c[] = {
	CLK_BLK_PERIC1_UID_USI07_SPI_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI07_SPI_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi08_usi[] = {
	CLK_BLK_PERIC1_UID_USI08_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI08_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi08_spi_i2c[] = {
	CLK_BLK_PERIC1_UID_USI08_SPI_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI08_SPI_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi09_i2c[] = {
	CLK_BLK_PERIC1_UID_USI09_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI09_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi10_i2c[] = {
	CLK_BLK_PERIC1_UID_USI10_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI10_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi09_usi[] = {
	CLK_BLK_PERIC1_UID_USI09_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI09_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi10_usi[] = {
	CLK_BLK_PERIC1_UID_USI10_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC1_UID_USI10_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_spi_multi_slv_q_ctrl_peric1[] = {
	CLK_BLK_PERIC1_UID_SPI_MULTI_SLV_Q_CTRL_PERIC1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_peric2_cmu_peric2[] = {
	CLK_BLK_PERIC2_UID_PERIC2_CMU_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric2[] = {
	CLK_BLK_PERIC2_UID_D_TZPC_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_peric2[] = {
	CLK_BLK_PERIC2_UID_GPIO_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_peric2[] = {
	CLK_BLK_PERIC2_UID_SLH_AXI_MI_P_PERIC2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peric2[] = {
	CLK_BLK_PERIC2_UID_SYSREG_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi00_spi_i2c[] = {
	CLK_BLK_PERIC2_UID_USI00_SPI_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI00_SPI_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi01_spi_i2c[] = {
	CLK_BLK_PERIC2_UID_USI01_SPI_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI01_SPI_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi02_i2c[] = {
	CLK_BLK_PERIC2_UID_USI02_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI02_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi03_i2c[] = {
	CLK_BLK_PERIC2_UID_USI03_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI03_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi05_i2c[] = {
	CLK_BLK_PERIC2_UID_USI05_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI05_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi06_i2c[] = {
	CLK_BLK_PERIC2_UID_USI06_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI06_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi00_usi[] = {
	CLK_BLK_PERIC2_UID_USI00_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI00_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi01_usi[] = {
	CLK_BLK_PERIC2_UID_USI01_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI01_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi02_usi[] = {
	CLK_BLK_PERIC2_UID_USI02_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI02_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi03_usi[] = {
	CLK_BLK_PERIC2_UID_USI03_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI03_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi05_usi[] = {
	CLK_BLK_PERIC2_UID_USI05_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI05_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi06_usi[] = {
	CLK_BLK_PERIC2_UID_USI06_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI06_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i3c06[] = {
	CLK_BLK_PERIC2_UID_I3C06_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C06_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c07[] = {
	CLK_BLK_PERIC2_UID_I3C07_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C07_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c08[] = {
	CLK_BLK_PERIC2_UID_I3C08_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C08_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c09[] = {
	CLK_BLK_PERIC2_UID_I3C09_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C09_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c10[] = {
	CLK_BLK_PERIC2_UID_I3C10_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C10_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c11[] = {
	CLK_BLK_PERIC2_UID_I3C11_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C11_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_spi_multi_slv_q_ctrl_peric2[] = {
	CLK_BLK_PERIC2_UID_SPI_MULTI_SLV_Q_CTRL_PERIC2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_usi11_i2c[] = {
	CLK_BLK_PERIC2_UID_USI11_I2C_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI11_I2C_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi11_usi[] = {
	CLK_BLK_PERIC2_UID_USI11_USI_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_USI11_USI_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i3c03[] = {
	CLK_BLK_PERIC2_UID_I3C03_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C03_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c04[] = {
	CLK_BLK_PERIC2_UID_I3C04_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C04_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_i3c05[] = {
	CLK_BLK_PERIC2_UID_I3C05_IPCLKPORT_I_PCLK,
	CLK_BLK_PERIC2_UID_I3C05_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_dbg_uart[] = {
	CLK_BLK_PERIC2_UID_DBG_UART_IPCLKPORT_PCLK,
	CLK_BLK_PERIC2_UID_DBG_UART_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_pwm[] = {
	CLK_BLK_PERIC2_UID_PWM_IPCLKPORT_I_PCLK_S0,
};
enum clk_id cmucal_vclk_ip_peris_cmu_peris[] = {
	CLK_BLK_PERIS_UID_PERIS_CMU_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peris[] = {
	CLK_BLK_PERIS_UID_D_TZPC_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gic[] = {
	CLK_BLK_PERIS_UID_GIC_IPCLKPORT_GICCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_ld_icc_cpugic_cluster0[] = {
	CLK_BLK_PERIS_UID_LH_AST_MI_LD_ICC_CPUGIC_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_peris[] = {
	CLK_BLK_PERIS_UID_SLH_AXI_MI_P_PERIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_perisgic[] = {
	CLK_BLK_PERIS_UID_SLH_AXI_MI_P_PERISGIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_iri_giccpu_cluster0[] = {
	CLK_BLK_PERIS_UID_LH_AST_SI_LD_IRI_GICCPU_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	CLK_BLK_PERIS_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	CLK_BLK_PERIS_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peris[] = {
	CLK_BLK_PERIS_UID_SYSREG_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu_sub[] = {
	CLK_BLK_PERIS_UID_TMU_SUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu_top[] = {
	CLK_BLK_PERIS_UID_TMU_TOP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt0[] = {
	CLK_BLK_PERIS_UID_WDT0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt1[] = {
	CLK_BLK_PERIS_UID_WDT1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_bira[] = {
	CLK_BLK_PERIS_UID_OTP_CON_BIRA_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_OTP_CON_BIRA_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_busif_ddd_peris[] = {
	CLK_BLK_PERIS_UID_BUSIF_DDD_PERIS_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_BUSIF_DDD_PERIS_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_ddd_peris[] = {
	CLK_BLK_PERIS_UID_DDD_PERIS_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_rgbp_cmu_rgbp[] = {
	CLK_BLK_RGBP_UID_RGBP_CMU_RGBP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_rgbp[] = {
	CLK_BLK_RGBP_UID_AD_APB_RGBP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_rgbp[] = {
	CLK_BLK_RGBP_UID_D_TZPC_RGBP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_rgbp[] = {
	CLK_BLK_RGBP_UID_SLH_AXI_MI_P_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d0_rgbp[] = {
	CLK_BLK_RGBP_UID_LH_AXI_SI_D0_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_rgbp[] = {
	CLK_BLK_RGBP_UID_PPMU_D0_RGBP_IPCLKPORT_ACLK,
	CLK_BLK_RGBP_UID_PPMU_D0_RGBP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_rgbp[] = {
	CLK_BLK_RGBP_UID_SYSMMU_D0_RGBP_IPCLKPORT_CLK_S2,
	CLK_BLK_RGBP_UID_SYSMMU_D0_RGBP_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysreg_rgbp[] = {
	CLK_BLK_RGBP_UID_SYSREG_RGBP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_rgbp[] = {
	CLK_BLK_RGBP_UID_XIU_D0_RGBP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_rgbp[] = {
	CLK_BLK_RGBP_UID_RGBP_IPCLKPORT_CLK,
	CLK_BLK_RGBP_UID_RGBP_IPCLKPORT_CLK_VOTF0,
	CLK_BLK_RGBP_UID_RGBP_IPCLKPORT_CLK_VOTF0,
	CLK_BLK_RGBP_UID_RGBP_IPCLKPORT_CLK_VOTF1,
	CLK_BLK_RGBP_UID_RGBP_IPCLKPORT_CLK_VOTF1,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf_byrprgbp[] = {
	CLK_BLK_RGBP_UID_LH_AST_MI_OTF_BYRPRGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf_rgbpmcfp[] = {
	CLK_BLK_RGBP_UID_LH_AST_SI_OTF_RGBPMCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_rgbp[] = {
	CLK_BLK_RGBP_UID_VGEN_LITE_RGBP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_rgbp[] = {
	CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_ACLK,
	CLK_BLK_RGBP_UID_PPMU_D1_RGBP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_d1_rgbp[] = {
	CLK_BLK_RGBP_UID_LH_AXI_SI_D1_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_rgbp[] = {
	CLK_BLK_RGBP_UID_XIU_D1_RGBP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_rgbp[] = {
	CLK_BLK_RGBP_UID_SYSMMU_D1_RGBP_IPCLKPORT_CLK_S1,
	CLK_BLK_RGBP_UID_SYSMMU_D1_RGBP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_rgbp[] = {
	CLK_BLK_RGBP_UID_SLH_ASTL_SI_G_PPMU_RGBP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_siu_g_ppmu_rgbp[] = {
	CLK_BLK_RGBP_UID_SIU_G_PPMU_RGBP_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_s2d_cmu_s2d[] = {
	CLK_BLK_S2D_UID_S2D_CMU_S2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_bis_s2d[] = {
	CLK_BLK_S2D_UID_BIS_S2D_IPCLKPORT_SCLK,
	CLK_BLK_S2D_UID_BIS_S2D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_g_scan2dram[] = {
	CLK_BLK_S2D_UID_SLH_AXI_MI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sdma_cmu_sdma[] = {
	CLK_BLK_SDMA_UID_SDMA_CMU_SDMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data5[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA5_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data4[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA4_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data7[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA7_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data6[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA6_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data1[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data0[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data3[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data2[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_DATA2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu1[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_MMU1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu2[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_MMU2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_sdma[] = {
	CLK_BLK_SDMA_UID_SLH_AXI_MI_P_SDMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_sdma[] = {
	CLK_BLK_SDMA_UID_SYSREG_SDMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu0[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_MMU0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu3[] = {
	CLK_BLK_SDMA_UID_LH_AXI_SI_LD_SDMADNC_MMU3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_sdma[] = {
	CLK_BLK_SDMA_UID_D_TZPC_SDMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_strm_sdmadsp0[] = {
	CLK_BLK_SDMA_UID_LH_AST_SI_LD_STRM_SDMADSP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_ld_strm_sdmadsp1[] = {
	CLK_BLK_SDMA_UID_LH_AST_SI_LD_STRM_SDMADSP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_sdma[] = {
	CLK_BLK_SDMA_UID_IP_SDMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_lp_dncsdma[] = {
	CLK_BLK_SDMA_UID_LH_AXI_MI_LP_DNCSDMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ssp_cmu_ssp[] = {
	CLK_BLK_SSP_UID_SSP_CMU_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_sss[] = {
	CLK_BLK_SSP_UID_BAAW_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_p_ssp[] = {
	CLK_BLK_SSP_UID_BPS_AXI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_ssp[] = {
	CLK_BLK_SSP_UID_D_TZPC_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_axi_mi_l_strong[] = {
	CLK_BLK_SSP_UID_LH_AXI_MI_L_STRONG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_ssp[] = {
	CLK_BLK_SSP_UID_SLH_AXI_MI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_acel_si_d_ssp[] = {
	CLK_BLK_SSP_UID_SLH_ACEL_SI_D_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ssp[] = {
	CLK_BLK_SSP_UID_PPMU_SSP_IPCLKPORT_ACLK,
	CLK_BLK_SSP_UID_PPMU_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_rtic[] = {
	CLK_BLK_SSP_UID_QE_RTIC_IPCLKPORT_ACLK,
	CLK_BLK_SSP_UID_QE_RTIC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_strong[] = {
	CLK_BLK_SSP_UID_QE_STRONG_IPCLKPORT_ACLK,
	CLK_BLK_SSP_UID_QE_STRONG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_sss[] = {
	CLK_BLK_SSP_UID_QE_SSS_IPCLKPORT_ACLK,
	CLK_BLK_SSP_UID_QE_SSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_rtic[] = {
	CLK_BLK_SSP_UID_RTIC_IPCLKPORT_I_ACLK,
	CLK_BLK_SSP_UID_RTIC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	CLK_BLK_SSP_UID_SSS_IPCLKPORT_I_ACLK,
	CLK_BLK_SSP_UID_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sweeper_d_ssp[] = {
	CLK_BLK_SSP_UID_SWEEPER_D_SSP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_ssp[] = {
	CLK_BLK_SSP_UID_SYSREG_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_ssp[] = {
	CLK_BLK_SSP_UID_VGEN_LITE_SSP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_ssp[] = {
	CLK_BLK_SSP_UID_XIU_D_SSP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_ssp[] = {
	CLK_BLK_SSP_UID_AD_APB_SYSMMU_SSP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_ssp[] = {
	CLK_BLK_SSP_UID_SYSMMU_SSP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_us_32to128_ssscore[] = {
	CLK_BLK_SSP_UID_US_32TO128_SSSCORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_adm_dap_g_sss[] = {
	CLK_BLK_SSP_UID_ADM_DAP_G_SSS_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_ssp[] = {
	CLK_BLK_SSP_UID_SLH_ASTL_SI_G_PPMU_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_strong_cmu_strong[] = {
	CLK_BLK_STRONG_UID_STRONG_CMU_STRONG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufd_cmu_ufd[] = {
	CLK_BLK_UFD_UID_UFD_CMU_UFD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_ufd[] = {
	CLK_BLK_UFD_UID_D_TZPC_UFD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_lp_cmgpufd[] = {
	CLK_BLK_UFD_UID_SLH_AXI_MI_LP_CMGPUFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_ufd[] = {
	CLK_BLK_UFD_UID_SLH_AXI_MI_P_UFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pdma_ufd[] = {
	CLK_BLK_UFD_UID_PDMA_UFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_ufd[] = {
	CLK_BLK_UFD_UID_SYSREG_UFD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ufd[] = {
	CLK_BLK_UFD_UID_UFD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_ufd[] = {
	CLK_BLK_UFD_UID_XIU_DP_UFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_spi_ufd[] = {
	CLK_BLK_UFD_UID_SPI_UFD_IPCLKPORT_PCLK,
	CLK_BLK_UFD_UID_SPI_UFD_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d_ufd[] = {
	CLK_BLK_UFD_UID_VGEN_LITE_D_UFD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d_ufd[] = {
	CLK_BLK_UFD_UID_SLH_AXI_SI_D_UFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_ufd[] = {
	CLK_BLK_UFD_UID_XIU_D_UFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_ld_ufddnc[] = {
	CLK_BLK_UFD_UID_SLH_AXI_SI_LD_UFDDNC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sram_miu_ufd[] = {
	CLK_BLK_UFD_UID_SRAM_MIU_UFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_i3c_ufd[] = {
	CLK_BLK_UFD_UID_I3C_UFD_IPCLKPORT_I_PCLK,
	CLK_BLK_UFD_UID_I3C_UFD_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_ufddnc[] = {
	CLK_BLK_UFD_UID_BAAW_D_UFDDNC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_ufd_secure[] = {
	CLK_BLK_UFD_UID_SYSREG_UFD_SECURE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_id_comp_ufd[] = {
	CLK_BLK_UFD_UID_ID_COMP_UFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_lp_ufdcsis[] = {
	CLK_BLK_UFD_UID_SLH_AXI_SI_LP_UFDCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_ast_mi_otf_csisufd[] = {
	CLK_BLK_UFD_UID_SLH_AST_MI_OTF_CSISUFD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ahb2axi_ahb0[] = {
	CLK_BLK_VTS_UID_AHB2AXI_AHB0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ahb2axi_ahb4[] = {
	CLK_BLK_VTS_UID_AHB2AXI_AHB4_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_asyncaxi0[] = {
	CLK_BLK_VTS_UID_ASYNCAXI0_IPCLKPORT_ACLKM,
	CLK_BLK_VTS_UID_ASYNCAXI0_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_asyncaxi2[] = {
	CLK_BLK_VTS_UID_ASYNCAXI2_IPCLKPORT_ACLKS,
	CLK_BLK_VTS_UID_ASYNCAXI2_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_asyncaxi4[] = {
	CLK_BLK_VTS_UID_ASYNCAXI4_IPCLKPORT_ACLKS,
	CLK_BLK_VTS_UID_ASYNCAXI4_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_baaw_c_vts[] = {
	CLK_BLK_VTS_UID_BAAW_C_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bps_lp_vts[] = {
	CLK_BLK_VTS_UID_BPS_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_p_vts[] = {
	CLK_BLK_VTS_UID_BPS_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dmailbox_test[] = {
	CLK_BLK_VTS_UID_DMAILBOX_TEST_IPCLKPORT_ACLK,
	CLK_BLK_VTS_UID_DMAILBOX_TEST_IPCLKPORT_PCLK,
	CLK_BLK_VTS_UID_DMAILBOX_TEST_IPCLKPORT_CCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb0[] = {
	CLK_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_HCLK,
	CLK_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb2[] = {
	CLK_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_HCLK,
	CLK_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb4[] = {
	CLK_BLK_VTS_UID_DMIC_AHB4_IPCLKPORT_HCLK,
	CLK_BLK_VTS_UID_DMIC_AHB4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if0[] = {
	CLK_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_PCLK,
	CLK_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_CLK,
	CLK_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_if1[] = {
	CLK_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_CLK,
	CLK_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_DIV2_CLK,
	CLK_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if2[] = {
	CLK_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_DMIC_IF_CLK,
	CLK_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_DMIC_IF_DIV2_CLK,
	CLK_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vts[] = {
	CLK_BLK_VTS_UID_D_TZPC_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_vts[] = {
	CLK_BLK_VTS_UID_GPIO_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic0[] = {
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic2[] = {
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK,
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic4[] = {
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK,
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK_BUS,
	CLK_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_intmem_code[] = {
	CLK_BLK_VTS_UID_INTMEM_CODE_IPCLKPORT_ACLK,
	CLK_BLK_VTS_UID_INTMEM_CODE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_intmem_data0[] = {
	CLK_BLK_VTS_UID_INTMEM_DATA0_IPCLKPORT_ACLK,
	CLK_BLK_VTS_UID_INTMEM_DATA0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_intmem_data1[] = {
	CLK_BLK_VTS_UID_INTMEM_DATA1_IPCLKPORT_ACLK,
	CLK_BLK_VTS_UID_INTMEM_DATA1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_intmem_pcm[] = {
	CLK_BLK_VTS_UID_INTMEM_PCM_IPCLKPORT_ACLK,
	CLK_BLK_VTS_UID_INTMEM_PCM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_audvts[] = {
	CLK_BLK_VTS_UID_SLH_AXI_MI_LD_AUDVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_lp_vts[] = {
	CLK_BLK_VTS_UID_SLH_AXI_MI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_vts[] = {
	CLK_BLK_VTS_UID_SLH_AXI_MI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_c_vts[] = {
	CLK_BLK_VTS_UID_SLH_AXI_SI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_abox_vts[] = {
	CLK_BLK_VTS_UID_MAILBOX_ABOX_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_vts[] = {
	CLK_BLK_VTS_UID_MAILBOX_AP_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pdma_vts[] = {
	CLK_BLK_VTS_UID_PDMA_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_serial_lif_vt[] = {
	CLK_BLK_VTS_UID_SERIAL_LIF_VT_IPCLKPORT_BCLK,
	CLK_BLK_VTS_UID_SERIAL_LIF_VT_IPCLKPORT_CCLK,
	CLK_BLK_VTS_UID_SERIAL_LIF_VT_IPCLKPORT_PCLK,
	CLK_BLK_VTS_UID_SERIAL_LIF_VT_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ss_vts_glue[] = {
	CLK_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK0,
	CLK_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK1,
	CLK_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK2,
	CLK_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_DIV2_CLK,
	CLK_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_ACLK_CPU,
};
enum clk_id cmucal_vclk_ip_sweeper_c_vts[] = {
	CLK_BLK_VTS_UID_SWEEPER_C_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vts[] = {
	CLK_BLK_VTS_UID_SYSREG_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite[] = {
	CLK_BLK_VTS_UID_VGEN_LITE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_wdt_vts[] = {
	CLK_BLK_VTS_UID_WDT_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ahb2axi_ahb2[] = {
	CLK_BLK_VTS_UID_AHB2AXI_AHB2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp1_vts[] = {
	CLK_BLK_VTS_UID_XIU_DP1_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vts_cmu_vts[] = {
	CLK_BLK_VTS_UID_VTS_CMU_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_ld_dncvts[] = {
	CLK_BLK_VTS_UID_SLH_AXI_MI_LD_DNCVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_timer[] = {
	CLK_BLK_VTS_UID_TIMER_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer1[] = {
	CLK_BLK_VTS_UID_TIMER1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer2[] = {
	CLK_BLK_VTS_UID_TIMER2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_yamin_mcu_vts[] = {
	CLK_BLK_VTS_UID_YAMIN_MCU_VTS_IPCLKPORT_CLKIN,
	CLK_BLK_VTS_UID_YAMIN_MCU_VTS_IPCLKPORT_IWICCLK,
	CLK_BLK_VTS_UID_YAMIN_MCU_VTS_IPCLKPORT_DBGCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_dnc_vts[] = {
	CLK_BLK_VTS_UID_MAILBOX_DNC_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_asyncaxi_serial_lif_vt[] = {
	CLK_BLK_VTS_UID_ASYNCAXI_SERIAL_LIF_VT_IPCLKPORT_ACLKS,
	CLK_BLK_VTS_UID_ASYNCAXI_SERIAL_LIF_VT_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_asyncinterrupt_vts[] = {
	CLK_BLK_VTS_UID_ASYNCINTERRUPT_VTS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_yuvp_cmu_yuvp[] = {
	CLK_BLK_YUVP_UID_YUVP_CMU_YUVP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_yuvp[] = {
	CLK_BLK_YUVP_UID_AD_APB_YUVP_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_slh_axi_mi_p_yuvp[] = {
	CLK_BLK_YUVP_UID_SLH_AXI_MI_P_YUVP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_yuvp[] = {
	CLK_BLK_YUVP_UID_SYSREG_YUVP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_slh_axi_si_d_yuvp[] = {
	CLK_BLK_YUVP_UID_SLH_AXI_SI_D_YUVP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_yuvp[] = {
	CLK_BLK_YUVP_UID_PPMU_YUVP_IPCLKPORT_ACLK,
	CLK_BLK_YUVP_UID_PPMU_YUVP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_yuvp[] = {
	CLK_BLK_YUVP_UID_SYSMMU_D_YUVP_IPCLKPORT_CLK_S1,
	CLK_BLK_YUVP_UID_SYSMMU_D_YUVP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d_yuvp[] = {
	CLK_BLK_YUVP_UID_XIU_D_YUVP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_yuvp[] = {
	CLK_BLK_YUVP_UID_YUVP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_mi_otf0_mcfpyuvp[] = {
	CLK_BLK_YUVP_UID_LH_AST_MI_OTF0_MCFPYUVP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf_yuvpdrcp[] = {
	CLK_BLK_YUVP_UID_LH_AST_SI_OTF_YUVPDRCP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_yuvp[] = {
	CLK_BLK_YUVP_UID_D_TZPC_YUVP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lh_ast_si_otf_yuvpmcfp[] = {
	CLK_BLK_YUVP_UID_LH_AST_SI_OTF_YUVPMCFP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_yuvp[] = {
	CLK_BLK_YUVP_UID_HPM_YUVP_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_busif_hpm_yuvp[] = {
	CLK_BLK_YUVP_UID_BUSIF_HPM_YUVP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_ddd_yuvp[] = {
	CLK_BLK_YUVP_UID_BUSIF_DDD_YUVP_IPCLKPORT_PCLK,
	CLK_BLK_YUVP_UID_BUSIF_DDD_YUVP_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_busif_add_yuvp[] = {
	CLK_BLK_YUVP_UID_BUSIF_ADD_YUVP_IPCLKPORT_PCLK,
	CLK_BLK_YUVP_UID_BUSIF_ADD_YUVP_IPCLKPORT_CLK_CORE,
	CLK_BLK_YUVP_UID_BUSIF_ADD_YUVP_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_add_yuvp[] = {
	CLK_BLK_YUVP_UID_ADD_YUVP_IPCLKPORT_CLK,
	CLK_BLK_YUVP_UID_ADD_YUVP_IPCLKPORT_CH_CLK,
};
enum clk_id cmucal_vclk_ip_ddd_yuvp[] = {
	CLK_BLK_YUVP_UID_DDD_YUVP_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_vgen_lite_yuvp[] = {
	CLK_BLK_YUVP_UID_VGEN_LITE_YUVP_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_slh_astl_si_g_ppmu_yuvp[] = {
	CLK_BLK_YUVP_UID_SLH_ASTL_SI_G_PPMU_YUVP_IPCLKPORT_I_CLK,
};

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_vclk_vddi_lut[] = {
	{400000, vddi_uud_lut_params},
	{300000, vddi_ud_lut_params},
	{200000, vddi_sud_lut_params},
	{100000, vddi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_mif_lut[] = {
	{4266000, vdd_mif_od_lut_params},
	{3080000, vdd_mif_ud_lut_params},
	{1690000, vdd_mif_sud_lut_params},
	{842000, vdd_mif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_g3d_lut[] = {
	{1300000, vdd_g3d_nm_lut_params},
	{1000000, vdd_g3d_ud_lut_params},
	{700000, vdd_g3d_sud_lut_params},
	{400000, vdd_g3d_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cam_lut[] = {
	{400000, vdd_cam_uud_lut_params},
	{300000, vdd_cam_ud_lut_params},
	{200000, vdd_cam_sud_lut_params},
	{100000, vdd_cam_nm_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl2_lut[] = {
	{3000000, vdd_cpucl2_sod_lut_params},
	{2500000, vdd_cpucl2_od_lut_params},
	{2000000, vdd_cpucl2_nm_lut_params},
	{1400000, vdd_cpucl2_ud_lut_params},
	{700000, vdd_cpucl2_sud_lut_params},
	{300000, vdd_cpucl2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl0_lut[] = {
	{2300000, vdd_cpucl0_sod_lut_params},
	{2050000, vdd_cpucl0_od_lut_params},
	{1800000, vdd_cpucl0_nm_lut_params},
	{1300000, vdd_cpucl0_ud_lut_params},
	{700000, vdd_cpucl0_sud_lut_params},
	{300000, vdd_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl1_lut[] = {
	{3000000, vdd_cpucl1_sod_lut_params},
	{2500000, vdd_cpucl1_od_lut_params},
	{2000000, vdd_cpucl1_nm_lut_params},
	{1400000, vdd_cpucl1_ud_lut_params},
	{700000, vdd_cpucl1_sud_lut_params},
	{300000, vdd_cpucl1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_alive_lut[] = {
	{300000, vdd_alive_ud_lut_params},
	{200000, vdd_alive_sud_lut_params},
	{100000, vdd_alive_nm_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_npu_lut[] = {
	{1550000, vdd_npu_od_lut_params},
	{1200000, vdd_npu_nm_lut_params},
	{800000, vdd_npu_ud_lut_params},
	{400000, vdd_npu_sud_lut_params},
	{200000, vdd_npu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_int_cmu_lut[] = {
	{808000, vdd_int_cmu_nm_lut_params},
	{808000, vdd_int_cmu_od_lut_params},
	{808000, vdd_int_cmu_sod_lut_params},
	{808000, vdd_int_cmu_ud_lut_params},
	{404000, vdd_int_cmu_sud_lut_params},
	{101000, vdd_int_cmu_uud_lut_params},
};

/* SPECIAL VCLK -> LUT List */
struct vclk_lut cmucal_vclk_mux_chub_timer_lut[] = {
	{76800, mux_chub_timer_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl0_cmuref_lut[] = {
	{200000, mux_cpucl0_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_cpu_lut[] = {
	{400000, mux_clkcmu_cmu_boost_cpu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl1_cmuref_lut[] = {
	{200000, mux_cpucl1_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl2_cmuref_lut[] = {
	{200000, mux_cpucl2_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_dsu_cmuref_lut[] = {
	{200000, mux_dsu_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_dsu_powerip_lut[] = {
	{2000000, mux_clk_dsu_powerip_sod_lut_params},
	{1750000, mux_clk_dsu_powerip_od_lut_params},
	{1400000, mux_clk_dsu_powerip_nm_lut_params},
	{1000000, mux_clk_dsu_powerip_ud_lut_params},
	{480000, mux_clk_dsu_powerip_sud_lut_params},
	{175000, mux_clk_dsu_powerip_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_hsi0_usb32drd_lut[] = {
	{76800, mux_clk_hsi0_usb32drd_ud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_hsi0_usb32drd_lut[] = {
	{76800, clkcmu_hsi0_usb32drd_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif_cmuref_lut[] = {
	{200000, mux_mif_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_mif_lut[] = {
	{400000, mux_clkcmu_cmu_boost_mif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_nocl0_cmuref_lut[] = {
	{200000, mux_nocl0_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_lut[] = {
	{400000, mux_clkcmu_cmu_boost_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_nocl1a_cmuref_lut[] = {
	{200000, mux_nocl1a_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_nocl1b_cmuref_lut[] = {
	{200000, mux_nocl1b_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_nocl2a_cmuref_lut[] = {
	{200000, mux_nocl2a_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_cam_lut[] = {
	{400000, mux_clkcmu_cmu_boost_cam_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_dpub_dsim_lut[] = {
	{533000, mux_clkcmu_dpub_dsim_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_hsi0_dposc_lut[] = {
	{76800, clkcmu_hsi0_dposc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_alive_spmi_lut[] = {
	{26869, div_clk_alive_spmi_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_alive_dbgcore_uart_lut[] = {
	{214950, div_clk_alive_dbgcore_uart_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_alive_pmu_sub_lut[] = {
	{429900, div_clk_alive_pmu_sub_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_dsif_lut[] = {
	{78000, div_clk_aud_dsif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_aud_cpu_lut[] = {
	{1066000, clkcmu_aud_cpu_sod_lut_params},
	{800000, clkcmu_aud_cpu_ud_lut_params},
	{533000, clkcmu_aud_cpu_sud_lut_params},
	{100000, clkcmu_aud_cpu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_aud_audif0_lut[] = {
	{78000, clkcmu_aud_audif0_uud_lut_params},
	{74444, clkcmu_aud_audif0_sod_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_aud_audif1_lut[] = {
	{78000, clkcmu_aud_audif1_uud_lut_params},
	{22581, clkcmu_aud_audif1_ud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_aud_serial_lif_lut[] = {
	{533000, div_clk_aud_serial_lif_ud_lut_params},
	{100000, div_clk_aud_serial_lif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clk_aud_mclk_lut[] = {
	{78000, clk_aud_mclk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_dmailbox_lut[] = {
	{76800, div_clk_chub_dmailbox_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkalive_chub_peri_lut[] = {
	{429900, clkalive_chub_peri_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi0_lut[] = {
	{429900, div_clk_chub_usi0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi1_lut[] = {
	{429900, div_clk_chub_usi1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi3_lut[] = {
	{429900, div_clk_chub_usi3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_usi2_lut[] = {
	{429900, div_clk_chub_usi2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_spi_ms_ctrl_lut[] = {
	{214950, div_clk_chub_spi_ms_ctrl_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_spi_i2c0_lut[] = {
	{214950, div_clk_chub_spi_i2c0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_chub_spi_i2c1_lut[] = {
	{214950, div_clk_chub_spi_i2c1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi4_lut[] = {
	{429900, div_clk_cmgp_usi4_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkalive_cmgp_peri_lut[] = {
	{429900, clkalive_cmgp_peri_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi1_lut[] = {
	{429900, div_clk_cmgp_usi1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi0_lut[] = {
	{429900, div_clk_cmgp_usi0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi2_lut[] = {
	{429900, div_clk_cmgp_usi2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi3_lut[] = {
	{429900, div_clk_cmgp_usi3_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi5_lut[] = {
	{429900, div_clk_cmgp_usi5_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi6_lut[] = {
	{429900, div_clk_cmgp_usi6_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_i3c_lut[] = {
	{214950, div_clk_cmgp_i3c_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_spi_ms_ctrl_lut[] = {
	{214950, div_clk_cmgp_spi_ms_ctrl_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_spi_i2c0_lut[] = {
	{214950, div_clk_cmgp_spi_i2c0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_spi_i2c1_lut[] = {
	{214950, div_clk_cmgp_spi_i2c1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_hpm_lut[] = {
	{400000, clkcmu_hpm_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clkcmu_cis_clk0_lut[] = {
	{100000, div_clkcmu_cis_clk0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clkcmu_cis_clk1_lut[] = {
	{100000, div_clkcmu_cis_clk1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clkcmu_cis_clk2_lut[] = {
	{100000, div_clkcmu_cis_clk2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clkcmu_cis_clk3_lut[] = {
	{100000, div_clkcmu_cis_clk3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clkcmu_cis_clk4_lut[] = {
	{100000, div_clkcmu_cis_clk4_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clkcmu_cis_clk5_lut[] = {
	{100000, div_clkcmu_cis_clk5_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clkcmu_cis_clk6_lut[] = {
	{100000, div_clkcmu_cis_clk6_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cis_clk7_lut[] = {
	{400000, mux_clkcmu_cis_clk7_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_core_1_lut[] = {
	{3000000, div_clk_cpucl1_core_1_sod_lut_params},
	{2500000, div_clk_cpucl1_core_1_od_lut_params},
	{2000000, div_clk_cpucl1_core_1_nm_lut_params},
	{1400000, div_clk_cpucl1_core_1_ud_lut_params},
	{700000, div_clk_cpucl1_core_1_sud_lut_params},
	{300000, div_clk_cpucl1_core_1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl2_htu_lut[] = {
	{750000, div_clk_cpucl2_htu_sod_lut_params},
	{625000, div_clk_cpucl2_htu_od_lut_params},
	{500000, div_clk_cpucl2_htu_nm_lut_params},
	{350000, div_clk_cpucl2_htu_ud_lut_params},
	{175000, div_clk_cpucl2_htu_sud_lut_params},
	{75000, div_clk_cpucl2_htu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clk_dnc_add_ch_clk_lut[] = {
	{8533, clk_dnc_add_ch_clk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clk_dsu_str_dem_clk_lut[] = {
	{7680, clk_dsu_str_dem_clk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric0_usi04_lut[] = {
	{400000, div_clk_peric0_usi04_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric0_i2c_lut[] = {
	{400000, mux_clk_peric0_i2c_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric1_uart_bt_lut[] = {
	{400000, mux_clk_peric1_uart_bt_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_usi07_lut[] = {
	{400000, div_clk_peric1_usi07_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_usi08_lut[] = {
	{400000, div_clk_peric1_usi08_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_usi09_lut[] = {
	{400000, div_clk_peric1_usi09_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_usi10_lut[] = {
	{400000, div_clk_peric1_usi10_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric1_spi_ms_ctrl_lut[] = {
	{400000, mux_clk_peric1_spi_ms_ctrl_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric1_usi07_spi_i2c_lut[] = {
	{400000, mux_clk_peric1_usi07_spi_i2c_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric1_usi08_spi_i2c_lut[] = {
	{400000, mux_clk_peric1_usi08_spi_i2c_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi00_lut[] = {
	{400000, div_clk_peric2_usi00_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi01_lut[] = {
	{400000, div_clk_peric2_usi01_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi02_lut[] = {
	{400000, div_clk_peric2_usi02_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi03_lut[] = {
	{400000, div_clk_peric2_usi03_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi05_lut[] = {
	{400000, div_clk_peric2_usi05_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi06_lut[] = {
	{400000, div_clk_peric2_usi06_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric2_spi_ms_ctrl_lut[] = {
	{400000, mux_clk_peric2_spi_ms_ctrl_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi11_lut[] = {
	{400000, div_clk_peric2_usi11_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric2_uart_dbg_lut[] = {
	{400000, mux_clk_peric2_uart_dbg_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric2_usi00_spi_i2c_lut[] = {
	{400000, mux_clk_peric2_usi00_spi_i2c_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_peric2_usi01_spi_i2c_lut[] = {
	{400000, mux_clk_peric2_usi01_spi_i2c_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_vts_serial_lif_lut[] = {
	{76800, div_clk_vts_serial_lif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_vts_dmic_lut[] = {
	{74444, clkcmu_vts_dmic_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_vts_dmailbox_cclk_lut[] = {
	{76800, div_clk_vts_dmailbox_cclk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clk_yuvp_add_ch_clk_lut[] = {
	{8533, clk_yuvp_add_ch_clk_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkalive_gnss_noc_lut[] = {
	{429900, mux_clkalive_gnss_noc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_dnc_noc_lut[] = {
	{936000, clkcmu_dnc_noc_sod_lut_params},
	{800000, clkcmu_dnc_noc_nm_lut_params},
	{533000, clkcmu_dnc_noc_ud_lut_params},
	{266500, clkcmu_dnc_noc_sud_lut_params},
	{78000, clkcmu_dnc_noc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkalive_dnc_noc_lut[] = {
	{429900, clkalive_dnc_noc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_mif_switch_lut[] = {
	{2028000, mux_clkcmu_mif_switch_ud_lut_params},
	{1690000, mux_clkcmu_mif_switch_sud_lut_params},
	{842000, mux_clkcmu_mif_switch_uud_lut_params},
};

/* COMMON VCLK -> LUT List */
struct vclk_lut cmucal_vclk_blk_cmu_lut[] = {
	{2028000, blk_cmu_sod_lut_params},
	{1066000, blk_cmu_nm_lut_params},
	{808000, blk_cmu_ud_lut_params},
	{800000, blk_cmu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_s2d_lut[] = {
	{200000, blk_s2d_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_alive_lut[] = {
	{429900, blk_alive_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_aud_lut[] = {
	{78000, blk_aud_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_chub_lut[] = {
	{429900, blk_chub_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cmgp_lut[] = {
	{214950, blk_cmgp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_lut[] = {
	{2300000, blk_cpucl0_sod_lut_params},
	{2050000, blk_cpucl0_od_lut_params},
	{1800000, blk_cpucl0_nm_lut_params},
	{1300000, blk_cpucl0_ud_lut_params},
	{700000, blk_cpucl0_sud_lut_params},
	{300000, blk_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl1_lut[] = {
	{3000000, blk_cpucl1_sod_lut_params},
	{2500000, blk_cpucl1_od_lut_params},
	{2000000, blk_cpucl1_nm_lut_params},
	{1400000, blk_cpucl1_ud_lut_params},
	{700000, blk_cpucl1_sud_lut_params},
	{300000, blk_cpucl1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl2_lut[] = {
	{3000000, blk_cpucl2_sod_lut_params},
	{2500000, blk_cpucl2_od_lut_params},
	{2000000, blk_cpucl2_nm_lut_params},
	{1400000, blk_cpucl2_ud_lut_params},
	{700000, blk_cpucl2_sud_lut_params},
	{300000, blk_cpucl2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dnc_lut[] = {
	{800000, blk_dnc_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dsu_lut[] = {
	{2000000, blk_dsu_sod_lut_params},
	{1750000, blk_dsu_od_lut_params},
	{1400000, blk_dsu_nm_lut_params},
	{1000000, blk_dsu_ud_lut_params},
	{480000, blk_dsu_sud_lut_params},
	{175000, blk_dsu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_g3dcore_lut[] = {
	{1300000, blk_g3dcore_nm_lut_params},
	{1000000, blk_g3dcore_ud_lut_params},
	{700000, blk_g3dcore_sud_lut_params},
	{400000, blk_g3dcore_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_gnpu_lut[] = {
	{800000, blk_gnpu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_gnpup_lut[] = {
	{800000, blk_gnpup_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_hsi0_lut[] = {
	{200000, blk_hsi0_ud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peric1_lut[] = {
	{400000, blk_peric1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peric2_lut[] = {
	{400000, blk_peric2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_sdma_lut[] = {
	{800000, blk_sdma_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_ufd_lut[] = {
	{429900, blk_ufd_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vts_lut[] = {
	{429900, blk_vts_ud_lut_params},
	{429900, blk_vts_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_glb_lut[] = {
	{400000, blk_cpucl0_glb_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dsp_lut[] = {
	{533000, blk_dsp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_m2m_lut[] = {
	{400000, blk_m2m_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mfc0_lut[] = {
	{350000, blk_mfc0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mfc1_lut[] = {
	{350000, blk_mfc1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_ssp_lut[] = {
	{200000, blk_ssp_nm_lut_params},
};

/* Switch VCLK -> LUT Parameter List */
struct switch_lut mux_clk_cpucl0_ddd_lut[] = {
	{1066000, 0, -1},
	{800000, 0, -1},
	{700000, 0, -1},
	{266500, 0, -1},
};
struct switch_lut mux_clk_cpucl1_ddd_0_lut[] = {
	{1066000, 0, -1},
	{700000, 0, -1},
	{266500, 0, -1},
};
struct switch_lut mux_clk_cpucl1_ddd_1_lut[] = {
	{1066000, 0, -1},
	{700000, 0, -1},
	{266500, 0, -1},
};
struct switch_lut mux_clk_cpucl1_ddd_2_lut[] = {
	{1066000, 0, -1},
	{700000, 0, -1},
	{266500, 0, -1},
};
struct switch_lut mux_clk_cpucl2_ddd_lut[] = {
	{1066000, 0, -1},
	{700000, 0, -1},
	{266500, 0, -1},
};
struct switch_lut mux_clk_dsu_ddd_lut[] = {
	{1066000, 0, -1},
	{800000, 0, -1},
	{468000, 0, -1},
	{167500, 0, -1},
};
struct switch_lut mux_clk_g3d_ddd_lut[] = {
	{1066000, 0, -1},
	{936000, 0, -1},
	{700000, 0, -1},
	{400000, 0, -1},
};
/*================================ SWPLL List =================================*/
struct vclk_switch switch_vdd_g3d[] = {
	{MUX_CLK_G3D_DDD, CLKCMU_G3D_SWITCH, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CLKCMU_G3D_SWITCH_USER, mux_clk_g3d_ddd_lut, 4},
};
struct vclk_switch switch_vdd_cpucl2[] = {
	{MUX_CLK_CPUCL2_DDD, CLKCMU_CPUCL2_SWITCH, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CLKCMU_CPUCL2_SWITCH_USER, mux_clk_cpucl2_ddd_lut, 3},
};
struct vclk_switch switch_vdd_cpucl0[] = {
	{MUX_CLK_CPUCL0_DDD, CLKCMU_CPUCL0_SWITCH, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CLKCMU_CPUCL0_SWITCH_USER, mux_clk_cpucl0_ddd_lut, 4},
	{MUX_CLK_DSU_DDD, CLKCMU_DSU_SWITCH, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CLKCMU_DSU_SWITCH_USER, mux_clk_dsu_ddd_lut, 4},
};
struct vclk_switch switch_vdd_cpucl1[] = {
	{MUX_CLK_CPUCL1_DDD_0, CLKCMU_CPUCL1_SWITCH, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CLKCMU_CPUCL1_SWITCH_USER, mux_clk_cpucl1_ddd_0_lut, 3},
	{MUX_CLK_CPUCL1_DDD_1, CLKCMU_CPUCL1_SWITCH, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CLKCMU_CPUCL1_SWITCH_USER, mux_clk_cpucl1_ddd_1_lut, 3},
	{MUX_CLK_CPUCL1_DDD_2, CLKCMU_CPUCL1_SWITCH, EMPTY_CAL_ID, EMPTY_CAL_ID, MUX_CLKCMU_CPUCL1_SWITCH_USER, mux_clk_cpucl1_ddd_2_lut, 3},
};

/*================================ VCLK List =================================*/
unsigned int cmucal_vclk_size = 1425;
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
	CMUCAL_VCLK(VCLK_VDDI, cmucal_vclk_vddi_lut, cmucal_vclk_vddi, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_MIF, cmucal_vclk_vdd_mif_lut, cmucal_vclk_vdd_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_G3D, cmucal_vclk_vdd_g3d_lut, cmucal_vclk_vdd_g3d, NULL, switch_vdd_g3d),
	CMUCAL_VCLK(VCLK_VDD_CAM, cmucal_vclk_vdd_cam_lut, cmucal_vclk_vdd_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CPUCL2, cmucal_vclk_vdd_cpucl2_lut, cmucal_vclk_vdd_cpucl2, NULL, switch_vdd_cpucl2),
	CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_vclk_vdd_cpucl0_lut, cmucal_vclk_vdd_cpucl0, NULL, switch_vdd_cpucl0),
	CMUCAL_VCLK(VCLK_VDD_CPUCL1, cmucal_vclk_vdd_cpucl1_lut, cmucal_vclk_vdd_cpucl1, NULL, switch_vdd_cpucl1),
	CMUCAL_VCLK(VCLK_VDD_ALIVE, cmucal_vclk_vdd_alive_lut, cmucal_vclk_vdd_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_NPU, cmucal_vclk_vdd_npu_lut, cmucal_vclk_vdd_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_INT_CMU, cmucal_vclk_vdd_int_cmu_lut, cmucal_vclk_vdd_int_cmu, NULL, NULL),

/* SPECIAL VCLK*/
	CMUCAL_VCLK(VCLK_MUX_CHUB_TIMER, cmucal_vclk_mux_chub_timer_lut, cmucal_vclk_mux_chub_timer, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL0_CMUREF, cmucal_vclk_mux_cpucl0_cmuref_lut, cmucal_vclk_mux_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST_CPU, cmucal_vclk_mux_clkcmu_cmu_boost_cpu_lut, cmucal_vclk_mux_clkcmu_cmu_boost_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL1_CMUREF, cmucal_vclk_mux_cpucl1_cmuref_lut, cmucal_vclk_mux_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL2_CMUREF, cmucal_vclk_mux_cpucl2_cmuref_lut, cmucal_vclk_mux_cpucl2_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_DSU_CMUREF, cmucal_vclk_mux_dsu_cmuref_lut, cmucal_vclk_mux_dsu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_DSU_POWERIP, cmucal_vclk_mux_clk_dsu_powerip_lut, cmucal_vclk_mux_clk_dsu_powerip, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_HSI0_USB32DRD, cmucal_vclk_mux_clk_hsi0_usb32drd_lut, cmucal_vclk_mux_clk_hsi0_usb32drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_HSI0_USB32DRD, cmucal_vclk_clkcmu_hsi0_usb32drd_lut, cmucal_vclk_clkcmu_hsi0_usb32drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF_CMUREF, cmucal_vclk_mux_mif_cmuref_lut, cmucal_vclk_mux_mif_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST_MIF, cmucal_vclk_mux_clkcmu_cmu_boost_mif_lut, cmucal_vclk_mux_clkcmu_cmu_boost_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_NOCL0_CMUREF, cmucal_vclk_mux_nocl0_cmuref_lut, cmucal_vclk_mux_nocl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST, cmucal_vclk_mux_clkcmu_cmu_boost_lut, cmucal_vclk_mux_clkcmu_cmu_boost, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_NOCL1A_CMUREF, cmucal_vclk_mux_nocl1a_cmuref_lut, cmucal_vclk_mux_nocl1a_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_NOCL1B_CMUREF, cmucal_vclk_mux_nocl1b_cmuref_lut, cmucal_vclk_mux_nocl1b_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_NOCL2A_CMUREF, cmucal_vclk_mux_nocl2a_cmuref_lut, cmucal_vclk_mux_nocl2a_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST_CAM, cmucal_vclk_mux_clkcmu_cmu_boost_cam_lut, cmucal_vclk_mux_clkcmu_cmu_boost_cam, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_DPUB_DSIM, cmucal_vclk_mux_clkcmu_dpub_dsim_lut, cmucal_vclk_mux_clkcmu_dpub_dsim, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_HSI0_DPOSC, cmucal_vclk_clkcmu_hsi0_dposc_lut, cmucal_vclk_clkcmu_hsi0_dposc, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_ALIVE_SPMI, cmucal_vclk_div_clk_alive_spmi_lut, cmucal_vclk_div_clk_alive_spmi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_ALIVE_DBGCORE_UART, cmucal_vclk_div_clk_alive_dbgcore_uart_lut, cmucal_vclk_div_clk_alive_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_ALIVE_PMU_SUB, cmucal_vclk_div_clk_alive_pmu_sub_lut, cmucal_vclk_div_clk_alive_pmu_sub, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_DSIF, cmucal_vclk_div_clk_aud_dsif_lut, cmucal_vclk_div_clk_aud_dsif, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_AUD_CPU, cmucal_vclk_clkcmu_aud_cpu_lut, cmucal_vclk_clkcmu_aud_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_AUD_AUDIF0, cmucal_vclk_clkcmu_aud_audif0_lut, cmucal_vclk_clkcmu_aud_audif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_AUD_AUDIF1, cmucal_vclk_clkcmu_aud_audif1_lut, cmucal_vclk_clkcmu_aud_audif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_AUD_SERIAL_LIF, cmucal_vclk_div_clk_aud_serial_lif_lut, cmucal_vclk_div_clk_aud_serial_lif, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLK_AUD_MCLK, cmucal_vclk_clk_aud_mclk_lut, cmucal_vclk_clk_aud_mclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_DMAILBOX, cmucal_vclk_div_clk_chub_dmailbox_lut, cmucal_vclk_div_clk_chub_dmailbox, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKALIVE_CHUB_PERI, cmucal_vclk_clkalive_chub_peri_lut, cmucal_vclk_clkalive_chub_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI0, cmucal_vclk_div_clk_chub_usi0_lut, cmucal_vclk_div_clk_chub_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI1, cmucal_vclk_div_clk_chub_usi1_lut, cmucal_vclk_div_clk_chub_usi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI3, cmucal_vclk_div_clk_chub_usi3_lut, cmucal_vclk_div_clk_chub_usi3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_USI2, cmucal_vclk_div_clk_chub_usi2_lut, cmucal_vclk_div_clk_chub_usi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_SPI_MS_CTRL, cmucal_vclk_div_clk_chub_spi_ms_ctrl_lut, cmucal_vclk_div_clk_chub_spi_ms_ctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_SPI_I2C0, cmucal_vclk_div_clk_chub_spi_i2c0_lut, cmucal_vclk_div_clk_chub_spi_i2c0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CHUB_SPI_I2C1, cmucal_vclk_div_clk_chub_spi_i2c1_lut, cmucal_vclk_div_clk_chub_spi_i2c1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI4, cmucal_vclk_div_clk_cmgp_usi4_lut, cmucal_vclk_div_clk_cmgp_usi4, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKALIVE_CMGP_PERI, cmucal_vclk_clkalive_cmgp_peri_lut, cmucal_vclk_clkalive_cmgp_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI1, cmucal_vclk_div_clk_cmgp_usi1_lut, cmucal_vclk_div_clk_cmgp_usi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI0, cmucal_vclk_div_clk_cmgp_usi0_lut, cmucal_vclk_div_clk_cmgp_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI2, cmucal_vclk_div_clk_cmgp_usi2_lut, cmucal_vclk_div_clk_cmgp_usi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI3, cmucal_vclk_div_clk_cmgp_usi3_lut, cmucal_vclk_div_clk_cmgp_usi3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI5, cmucal_vclk_div_clk_cmgp_usi5_lut, cmucal_vclk_div_clk_cmgp_usi5, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI6, cmucal_vclk_div_clk_cmgp_usi6_lut, cmucal_vclk_div_clk_cmgp_usi6, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_I3C, cmucal_vclk_div_clk_cmgp_i3c_lut, cmucal_vclk_div_clk_cmgp_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_SPI_MS_CTRL, cmucal_vclk_div_clk_cmgp_spi_ms_ctrl_lut, cmucal_vclk_div_clk_cmgp_spi_ms_ctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_SPI_I2C0, cmucal_vclk_div_clk_cmgp_spi_i2c0_lut, cmucal_vclk_div_clk_cmgp_spi_i2c0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_SPI_I2C1, cmucal_vclk_div_clk_cmgp_spi_i2c1_lut, cmucal_vclk_div_clk_cmgp_spi_i2c1, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_HPM, cmucal_vclk_clkcmu_hpm_lut, cmucal_vclk_clkcmu_hpm, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_CIS_CLK0, cmucal_vclk_div_clkcmu_cis_clk0_lut, cmucal_vclk_div_clkcmu_cis_clk0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_CIS_CLK1, cmucal_vclk_div_clkcmu_cis_clk1_lut, cmucal_vclk_div_clkcmu_cis_clk1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_CIS_CLK2, cmucal_vclk_div_clkcmu_cis_clk2_lut, cmucal_vclk_div_clkcmu_cis_clk2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_CIS_CLK3, cmucal_vclk_div_clkcmu_cis_clk3_lut, cmucal_vclk_div_clkcmu_cis_clk3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_CIS_CLK4, cmucal_vclk_div_clkcmu_cis_clk4_lut, cmucal_vclk_div_clkcmu_cis_clk4, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_CIS_CLK5, cmucal_vclk_div_clkcmu_cis_clk5_lut, cmucal_vclk_div_clkcmu_cis_clk5, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLKCMU_CIS_CLK6, cmucal_vclk_div_clkcmu_cis_clk6_lut, cmucal_vclk_div_clkcmu_cis_clk6, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CIS_CLK7, cmucal_vclk_mux_clkcmu_cis_clk7_lut, cmucal_vclk_mux_clkcmu_cis_clk7, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_CORE_1, cmucal_vclk_div_clk_cpucl1_core_1_lut, cmucal_vclk_div_clk_cpucl1_core_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL2_HTU, cmucal_vclk_div_clk_cpucl2_htu_lut, cmucal_vclk_div_clk_cpucl2_htu, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLK_DNC_ADD_CH_CLK, cmucal_vclk_clk_dnc_add_ch_clk_lut, cmucal_vclk_clk_dnc_add_ch_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLK_DSU_STR_DEM_CLK, cmucal_vclk_clk_dsu_str_dem_clk_lut, cmucal_vclk_clk_dsu_str_dem_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI04, cmucal_vclk_div_clk_peric0_usi04_lut, cmucal_vclk_div_clk_peric0_usi04, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC0_I2C, cmucal_vclk_mux_clk_peric0_i2c_lut, cmucal_vclk_mux_clk_peric0_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC1_UART_BT, cmucal_vclk_mux_clk_peric1_uart_bt_lut, cmucal_vclk_mux_clk_peric1_uart_bt, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI07, cmucal_vclk_div_clk_peric1_usi07_lut, cmucal_vclk_div_clk_peric1_usi07, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI08, cmucal_vclk_div_clk_peric1_usi08_lut, cmucal_vclk_div_clk_peric1_usi08, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI09, cmucal_vclk_div_clk_peric1_usi09_lut, cmucal_vclk_div_clk_peric1_usi09, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI10, cmucal_vclk_div_clk_peric1_usi10_lut, cmucal_vclk_div_clk_peric1_usi10, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC1_SPI_MS_CTRL, cmucal_vclk_mux_clk_peric1_spi_ms_ctrl_lut, cmucal_vclk_mux_clk_peric1_spi_ms_ctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC1_USI07_SPI_I2C, cmucal_vclk_mux_clk_peric1_usi07_spi_i2c_lut, cmucal_vclk_mux_clk_peric1_usi07_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC1_USI08_SPI_I2C, cmucal_vclk_mux_clk_peric1_usi08_spi_i2c_lut, cmucal_vclk_mux_clk_peric1_usi08_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI00, cmucal_vclk_div_clk_peric2_usi00_lut, cmucal_vclk_div_clk_peric2_usi00, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI01, cmucal_vclk_div_clk_peric2_usi01_lut, cmucal_vclk_div_clk_peric2_usi01, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI02, cmucal_vclk_div_clk_peric2_usi02_lut, cmucal_vclk_div_clk_peric2_usi02, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI03, cmucal_vclk_div_clk_peric2_usi03_lut, cmucal_vclk_div_clk_peric2_usi03, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI05, cmucal_vclk_div_clk_peric2_usi05_lut, cmucal_vclk_div_clk_peric2_usi05, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI06, cmucal_vclk_div_clk_peric2_usi06_lut, cmucal_vclk_div_clk_peric2_usi06, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC2_SPI_MS_CTRL, cmucal_vclk_mux_clk_peric2_spi_ms_ctrl_lut, cmucal_vclk_mux_clk_peric2_spi_ms_ctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI11, cmucal_vclk_div_clk_peric2_usi11_lut, cmucal_vclk_div_clk_peric2_usi11, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC2_UART_DBG, cmucal_vclk_mux_clk_peric2_uart_dbg_lut, cmucal_vclk_mux_clk_peric2_uart_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC2_USI00_SPI_I2C, cmucal_vclk_mux_clk_peric2_usi00_spi_i2c_lut, cmucal_vclk_mux_clk_peric2_usi00_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_PERIC2_USI01_SPI_I2C, cmucal_vclk_mux_clk_peric2_usi01_spi_i2c_lut, cmucal_vclk_mux_clk_peric2_usi01_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_VTS_SERIAL_LIF, cmucal_vclk_div_clk_vts_serial_lif_lut, cmucal_vclk_div_clk_vts_serial_lif, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_VTS_DMIC, cmucal_vclk_clkcmu_vts_dmic_lut, cmucal_vclk_clkcmu_vts_dmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_VTS_DMAILBOX_CCLK, cmucal_vclk_div_clk_vts_dmailbox_cclk_lut, cmucal_vclk_div_clk_vts_dmailbox_cclk, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLK_YUVP_ADD_CH_CLK, cmucal_vclk_clk_yuvp_add_ch_clk_lut, cmucal_vclk_clk_yuvp_add_ch_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKALIVE_GNSS_NOC, cmucal_vclk_mux_clkalive_gnss_noc_lut, cmucal_vclk_mux_clkalive_gnss_noc, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_DNC_NOC, cmucal_vclk_clkcmu_dnc_noc_lut, cmucal_vclk_clkcmu_dnc_noc, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKALIVE_DNC_NOC, cmucal_vclk_clkalive_dnc_noc_lut, cmucal_vclk_clkalive_dnc_noc, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_MIF_SWITCH, cmucal_vclk_mux_clkcmu_mif_switch_lut, cmucal_vclk_mux_clkcmu_mif_switch, NULL, NULL),

/* COMMON VCLK*/
	CMUCAL_VCLK(VCLK_BLK_CMU, cmucal_vclk_blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_S2D, cmucal_vclk_blk_s2d_lut, cmucal_vclk_blk_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ALIVE, cmucal_vclk_blk_alive_lut, cmucal_vclk_blk_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_AUD, cmucal_vclk_blk_aud_lut, cmucal_vclk_blk_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CHUB, cmucal_vclk_blk_chub_lut, cmucal_vclk_blk_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMGP, cmucal_vclk_blk_cmgp_lut, cmucal_vclk_blk_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, cmucal_vclk_blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL1, cmucal_vclk_blk_cpucl1_lut, cmucal_vclk_blk_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL2, cmucal_vclk_blk_cpucl2_lut, cmucal_vclk_blk_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DNC, cmucal_vclk_blk_dnc_lut, cmucal_vclk_blk_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DSU, cmucal_vclk_blk_dsu_lut, cmucal_vclk_blk_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G3DCORE, cmucal_vclk_blk_g3dcore_lut, cmucal_vclk_blk_g3dcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_GNPU, cmucal_vclk_blk_gnpu_lut, cmucal_vclk_blk_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_GNPUP, cmucal_vclk_blk_gnpup_lut, cmucal_vclk_blk_gnpup, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_HSI0, cmucal_vclk_blk_hsi0_lut, cmucal_vclk_blk_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIC1, cmucal_vclk_blk_peric1_lut, cmucal_vclk_blk_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIC2, cmucal_vclk_blk_peric2_lut, cmucal_vclk_blk_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_SDMA, cmucal_vclk_blk_sdma_lut, cmucal_vclk_blk_sdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_UFD, cmucal_vclk_blk_ufd_lut, cmucal_vclk_blk_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VTS, cmucal_vclk_blk_vts_lut, cmucal_vclk_blk_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0_GLB, cmucal_vclk_blk_cpucl0_glb_lut, cmucal_vclk_blk_cpucl0_glb, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DSP, cmucal_vclk_blk_dsp_lut, cmucal_vclk_blk_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_M2M, cmucal_vclk_blk_m2m_lut, cmucal_vclk_blk_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC0, cmucal_vclk_blk_mfc0_lut, cmucal_vclk_blk_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC1, cmucal_vclk_blk_mfc1_lut, cmucal_vclk_blk_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_SSP, cmucal_vclk_blk_ssp_lut, cmucal_vclk_blk_ssp, NULL, NULL),

/* GATE VCLK*/
	CMUCAL_VCLK(VCLK_IP_ALIVE_CMU_ALIVE, NULL, cmucal_vclk_ip_alive_cmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_ALIVE, NULL, cmucal_vclk_ip_apbif_pmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTC, NULL, cmucal_vclk_ip_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_INTCOMB_VGPIO2AP, NULL, cmucal_vclk_ip_apbif_intcomb_vgpio2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_INTCOMB_VGPIO2APM, NULL, cmucal_vclk_ip_apbif_intcomb_vgpio2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_INTCOMB_VGPIO2PMU, NULL, cmucal_vclk_ip_apbif_intcomb_vgpio2pmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TOP_RTC, NULL, cmucal_vclk_ip_top_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLKMON, NULL, cmucal_vclk_ip_clkmon, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DBGCORE_UART, NULL, cmucal_vclk_ip_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DTZPC_ALIVE, NULL, cmucal_vclk_ip_dtzpc_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION, NULL, cmucal_vclk_ip_grebeintegration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HW_SCANDUMP_CLKSTOP_CTRL, NULL, cmucal_vclk_ip_hw_scandump_clkstop_ctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPMI_MASTER_PMIC, NULL, cmucal_vclk_ip_spmi_master_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_MODEM, NULL, cmucal_vclk_ip_slh_axi_mi_c_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_VTS, NULL, cmucal_vclk_ip_slh_axi_mi_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_APM, NULL, cmucal_vclk_ip_slh_axi_mi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_C_CMGP, NULL, cmucal_vclk_ip_slh_axi_si_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_APM, NULL, cmucal_vclk_ip_lh_axi_si_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LP_VTS, NULL, cmucal_vclk_ip_slh_axi_si_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_AP, NULL, cmucal_vclk_ip_mailbox_apm_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CP, NULL, cmucal_vclk_ip_mailbox_apm_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP, NULL, cmucal_vclk_ip_mailbox_ap_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP_S, NULL, cmucal_vclk_ip_mailbox_ap_cp_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_DBGCORE, NULL, cmucal_vclk_ip_mailbox_ap_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU_INTR_GEN, NULL, cmucal_vclk_ip_pmu_intr_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ROM_CRC32_HOST, NULL, cmucal_vclk_ip_rom_crc32_host, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_P_ALIVE, NULL, cmucal_vclk_ip_sweeper_p_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ALIVE, NULL, cmucal_vclk_ip_sysreg_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_ALIVE, NULL, cmucal_vclk_ip_vgen_lite_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_ALIVE, NULL, cmucal_vclk_ip_wdt_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_ALIVE, NULL, cmucal_vclk_ip_xiu_dp_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_RTC, NULL, cmucal_vclk_ip_chub_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_CHUB, NULL, cmucal_vclk_ip_slh_axi_mi_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_GNSS, NULL, cmucal_vclk_ip_slh_axi_mi_c_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LP_CHUB, NULL, cmucal_vclk_ip_slh_axi_si_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CHUB, NULL, cmucal_vclk_ip_mailbox_apm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_GNSS, NULL, cmucal_vclk_ip_mailbox_apm_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_VTS, NULL, cmucal_vclk_ip_mailbox_apm_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CHUB, NULL, cmucal_vclk_ip_mailbox_ap_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_GNSS, NULL, cmucal_vclk_ip_mailbox_ap_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_CHUB, NULL, cmucal_vclk_ip_mailbox_cp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CP_GNSS, NULL, cmucal_vclk_ip_mailbox_cp_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_GNSS_CHUB, NULL, cmucal_vclk_ip_mailbox_gnss_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_SHARED_SRAM, NULL, cmucal_vclk_ip_mailbox_shared_sram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_VTS_CHUB, NULL, cmucal_vclk_ip_mailbox_vts_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_PPU_ALIVE_CPUCL0, NULL, cmucal_vclk_ip_slh_axi_si_ppu_alive_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM, NULL, cmucal_vclk_ip_intmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU, NULL, cmucal_vclk_ip_pmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_IP_APM, NULL, cmucal_vclk_ip_slh_axi_si_ip_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_ID_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_mi_id_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_ALIVEDNC, NULL, cmucal_vclk_ip_slh_axi_si_p_alivednc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPC_ALIVE, NULL, cmucal_vclk_ip_spc_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APM_DMA, NULL, cmucal_vclk_ip_apm_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_ALIVE, NULL, cmucal_vclk_ip_apbif_gpio_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT_ALIVE, NULL, cmucal_vclk_ip_mct_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DFT_ALIVE, NULL, cmucal_vclk_ip_dft_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AUD_CMU_AUD, NULL, cmucal_vclk_ip_aud_cmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD, NULL, cmucal_vclk_ip_ad_apb_smmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD_NS1, NULL, cmucal_vclk_ip_ad_apb_smmu_aud_ns1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD_S, NULL, cmucal_vclk_ip_ad_apb_smmu_aud_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_US_32TO128, NULL, cmucal_vclk_ip_axi_us_32to128, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_AUDVTS, NULL, cmucal_vclk_ip_baaw_d_audvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_AUD, NULL, cmucal_vclk_ip_d_tzpc_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_HSI0AUD, NULL, cmucal_vclk_ip_slh_axi_mi_ld_hsi0aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_AUD, NULL, cmucal_vclk_ip_slh_axi_mi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_QDI_SI_D_AUD, NULL, cmucal_vclk_ip_lh_qdi_si_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LD_AUDVTS, NULL, cmucal_vclk_ip_slh_axi_si_ld_audvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD0, NULL, cmucal_vclk_ip_mailbox_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD1, NULL, cmucal_vclk_ip_mailbox_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD2, NULL, cmucal_vclk_ip_mailbox_aud2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD3, NULL, cmucal_vclk_ip_mailbox_aud3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_AUD, NULL, cmucal_vclk_ip_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_AUD, NULL, cmucal_vclk_ip_smmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_AUD, NULL, cmucal_vclk_ip_sysreg_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_AUD, NULL, cmucal_vclk_ip_trex_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LD_AUDHSI0, NULL, cmucal_vclk_ip_slh_axi_si_ld_audhsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_AUD, NULL, cmucal_vclk_ip_vgen_lite_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_LITE, NULL, cmucal_vclk_ip_ad_apb_vgen_lite, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SERIAL_LIF, NULL, cmucal_vclk_ip_serial_lif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DFTMUX_AUD, NULL, cmucal_vclk_ip_dftmux_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_AUD, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD0, NULL, cmucal_vclk_ip_dmic_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD1, NULL, cmucal_vclk_ip_dmic_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD2, NULL, cmucal_vclk_ip_dmic_aud2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BYRP_CMU_BYRP, NULL, cmucal_vclk_ip_byrp_cmu_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_BYRP, NULL, cmucal_vclk_ip_ad_apb_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BYRP, NULL, cmucal_vclk_ip_d_tzpc_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_BYRP, NULL, cmucal_vclk_ip_lh_axi_si_d_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_BYRP, NULL, cmucal_vclk_ip_ppmu_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_BYRP, NULL, cmucal_vclk_ip_sysmmu_d_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BYRP, NULL, cmucal_vclk_ip_sysreg_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_BYRP, NULL, cmucal_vclk_ip_xiu_d_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_BYRP, NULL, cmucal_vclk_ip_slh_axi_mi_p_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF_BYRPRGBP, NULL, cmucal_vclk_ip_lh_ast_si_otf_byrprgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIPU_BYRP, NULL, cmucal_vclk_ip_sipu_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_BYRP, NULL, cmucal_vclk_ip_vgen_lite_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_BYRP, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_CMU_CHUB, NULL, cmucal_vclk_ip_chub_cmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_P_CHUB, NULL, cmucal_vclk_ip_bps_axi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CM4_CHUB, NULL, cmucal_vclk_ip_cm4_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMAILBOX_CHUB, NULL, cmucal_vclk_ip_dmailbox_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LP_CHUB, NULL, cmucal_vclk_ip_slh_axi_mi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CHUB, NULL, cmucal_vclk_ip_slh_axi_mi_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_C_CHUB, NULL, cmucal_vclk_ip_slh_axi_si_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM_CHUB, NULL, cmucal_vclk_ip_pwm_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CHUB, NULL, cmucal_vclk_ip_sysreg_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER_CHUB, NULL, cmucal_vclk_ip_timer_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_CHUB, NULL, cmucal_vclk_ip_wdt_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CHUB, NULL, cmucal_vclk_ip_xiu_dp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CHUB, NULL, cmucal_vclk_ip_i2c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_CHUB, NULL, cmucal_vclk_ip_i3c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB0, NULL, cmucal_vclk_ip_usi_chub0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB1, NULL, cmucal_vclk_ip_usi_chub1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB3, NULL, cmucal_vclk_ip_usi_chub3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CHUB2, NULL, cmucal_vclk_ip_usi_chub2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_LP_CHUB, NULL, cmucal_vclk_ip_bps_axi_lp_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_CHUB_COMBINE_WAKEUP_SRC, NULL, cmucal_vclk_ip_apbif_chub_combine_wakeup_src, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_C_CHUB, NULL, cmucal_vclk_ip_sweeper_c_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_CHUB, NULL, cmucal_vclk_ip_apbif_gpio_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2AP, NULL, cmucal_vclk_ip_sysreg_combine_chub2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_COMBINE_CHUB2APM, NULL, cmucal_vclk_ip_sysreg_combine_chub2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCINTERRUPT, NULL, cmucal_vclk_ip_asyncinterrupt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_DNCCHUB, NULL, cmucal_vclk_ip_slh_axi_mi_ld_dncchub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_LD_DNCCHUB, NULL, cmucal_vclk_ip_bps_axi_ld_dncchub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_CHUB_DNC, NULL, cmucal_vclk_ip_mailbox_chub_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_I2C_CHUB0, NULL, cmucal_vclk_ip_spi_i2c_chub0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_I2C_CHUB1, NULL, cmucal_vclk_ip_spi_i2c_chub1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_MULTI_SLV_Q_CTRL_CHUB, NULL, cmucal_vclk_ip_spi_multi_slv_q_ctrl_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CHUB_ALV, NULL, cmucal_vclk_ip_chub_alv, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_UPMU_CHUB, NULL, cmucal_vclk_ip_apbif_upmu_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_CMU_CMGP, NULL, cmucal_vclk_ip_cmgp_cmu_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_CMGP, NULL, cmucal_vclk_ip_apbif_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CMGP, NULL, cmucal_vclk_ip_d_tzpc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_I2C_CMGP0, NULL, cmucal_vclk_ip_spi_i2c_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_I2C_CMGP1, NULL, cmucal_vclk_ip_spi_i2c_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP2, NULL, cmucal_vclk_ip_i2c_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP3, NULL, cmucal_vclk_ip_i2c_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_CMGP, NULL, cmucal_vclk_ip_i3c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_C_CMGP, NULL, cmucal_vclk_ip_slh_axi_mi_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP, NULL, cmucal_vclk_ip_sysreg_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2APM, NULL, cmucal_vclk_ip_sysreg_cmgp2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CP, NULL, cmucal_vclk_ip_sysreg_cmgp2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_AP, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP0, NULL, cmucal_vclk_ip_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP1, NULL, cmucal_vclk_ip_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP2, NULL, cmucal_vclk_ip_usi_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP3, NULL, cmucal_vclk_ip_usi_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CHUB, NULL, cmucal_vclk_ip_sysreg_cmgp2chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2GNSS, NULL, cmucal_vclk_ip_sysreg_cmgp2gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP4, NULL, cmucal_vclk_ip_i2c_cmgp4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP5, NULL, cmucal_vclk_ip_i2c_cmgp5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP6, NULL, cmucal_vclk_ip_i2c_cmgp6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP4, NULL, cmucal_vclk_ip_usi_cmgp4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP5, NULL, cmucal_vclk_ip_usi_cmgp5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP6, NULL, cmucal_vclk_ip_usi_cmgp6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_MULTI_SLV_Q_CTRL_CMGP, NULL, cmucal_vclk_ip_spi_multi_slv_q_ctrl_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LP_CMGPUFD, NULL, cmucal_vclk_ip_slh_axi_si_lp_cmgpufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_I2C, NULL, cmucal_vclk_ip_cmgp_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_CPUCL0_0, NULL, cmucal_vclk_ip_add_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_CPUCL0_0, NULL, cmucal_vclk_ip_busif_add_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_0, NULL, cmucal_vclk_ip_busif_str_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL0, NULL, cmucal_vclk_ip_htu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0, NULL, cmucal_vclk_ip_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_STR_CPUCL0_0, NULL, cmucal_vclk_ip_str_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CLUSTER0, NULL, cmucal_vclk_ip_adm_apb_g_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_GLB_CMU_CPUCL0_GLB, NULL, cmucal_vclk_ip_cpucl0_glb_cmu_cpucl0_glb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0_0, NULL, cmucal_vclk_ip_hpm_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0_1, NULL, cmucal_vclk_ip_hpm_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0_2, NULL, cmucal_vclk_ip_hpm_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_P_CSSYS, NULL, cmucal_vclk_ip_apb_async_p_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_CPUCL0, NULL, cmucal_vclk_ip_bps_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_CPUCL0, NULL, cmucal_vclk_ip_busif_hpm_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS, NULL, cmucal_vclk_ip_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL0, NULL, cmucal_vclk_ip_d_tzpc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_IT_CLUSTER0, NULL, cmucal_vclk_ip_lh_atb_mi_it_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_T_BDU, NULL, cmucal_vclk_ip_lh_atb_mi_t_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_G_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_mi_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_IG_CSSYS, NULL, cmucal_vclk_ip_slh_axi_mi_ig_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_IG_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_mi_ig_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_IG_ETR, NULL, cmucal_vclk_ip_slh_axi_mi_ig_etr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_IG_STM, NULL, cmucal_vclk_ip_slh_axi_mi_ig_stm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CPUCL0, NULL, cmucal_vclk_ip_slh_axi_mi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_G_CSSYS, NULL, cmucal_vclk_ip_lh_axi_si_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_IG_CSSYS, NULL, cmucal_vclk_ip_slh_axi_si_ig_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_IG_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_si_ig_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_IG_ETR, NULL, cmucal_vclk_ip_slh_axi_si_ig_etr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_CPUCL0, NULL, cmucal_vclk_ip_trex_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CSSYS, NULL, cmucal_vclk_ip_xiu_dp_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_CPUCL0, NULL, cmucal_vclk_ip_xiu_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_IG_STM, NULL, cmucal_vclk_ip_slh_axi_si_ig_stm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU_PCSM_PM, NULL, cmucal_vclk_ip_pmu_pcsm_pm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDC_CPUCL0_0, NULL, cmucal_vclk_ip_busif_ddc_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDC_CPUCL0_1, NULL, cmucal_vclk_ip_busif_ddc_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_IT_DDCDSU, NULL, cmucal_vclk_ip_lh_atb_mi_it_ddcdsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_T_DDCG3D, NULL, cmucal_vclk_ip_lh_atb_mi_t_ddcg3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_IT_DDCLIT, NULL, cmucal_vclk_ip_lh_atb_mi_it_ddclit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_IT_DDCMID0, NULL, cmucal_vclk_ip_lh_atb_mi_it_ddcmid0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_IT_DDCMID1, NULL, cmucal_vclk_ip_lh_atb_mi_it_ddcmid1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_IT_DDCMID2, NULL, cmucal_vclk_ip_lh_atb_mi_it_ddcmid2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MI_IT_DDCBIG, NULL, cmucal_vclk_ip_lh_atb_mi_it_ddcbig, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CFM_CPUCL0, NULL, cmucal_vclk_ip_cfm_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_CPUCL0_1, NULL, cmucal_vclk_ip_add_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_CPUCL0_1, NULL, cmucal_vclk_ip_busif_add_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_1, NULL, cmucal_vclk_ip_busif_str_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL1_0, NULL, cmucal_vclk_ip_htu_cpucl1_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1_CMU_CPUCL1, NULL, cmucal_vclk_ip_cpucl1_cmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL1_1, NULL, cmucal_vclk_ip_htu_cpucl1_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL1_2, NULL, cmucal_vclk_ip_htu_cpucl1_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_STR_CPUCL0_1, NULL, cmucal_vclk_ip_str_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_CPUCL0_2, NULL, cmucal_vclk_ip_add_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_CPUCL0_2, NULL, cmucal_vclk_ip_busif_add_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_2, NULL, cmucal_vclk_ip_busif_str_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL2, NULL, cmucal_vclk_ip_htu_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL2_CMU_CPUCL2, NULL, cmucal_vclk_ip_cpucl2_cmu_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_STR_CPUCL0_2, NULL, cmucal_vclk_ip_str_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIPI_PHY_LINK_WRAP, NULL, cmucal_vclk_ip_mipi_phy_link_wrap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_CSIS, NULL, cmucal_vclk_ip_sysmmu_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS_CMU_CSIS, NULL, cmucal_vclk_ip_csis_cmu_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS_WDMA, NULL, cmucal_vclk_ip_ad_apb_csis_wdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS_PDP, NULL, cmucal_vclk_ip_csis_pdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CSIS, NULL, cmucal_vclk_ip_d_tzpc_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_OIS_MCU_TOP, NULL, cmucal_vclk_ip_ad_axi_ois_mcu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_CSIS, NULL, cmucal_vclk_ip_xiu_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D2_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OIS_MCU_TOP, NULL, cmucal_vclk_ip_ois_mcu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0, NULL, cmucal_vclk_ip_ppmu_d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1, NULL, cmucal_vclk_ip_ppmu_d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D2, NULL, cmucal_vclk_ip_ppmu_d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_WDMA0, NULL, cmucal_vclk_ip_qe_csis_wdma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_WDMA1, NULL, cmucal_vclk_ip_qe_csis_wdma1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_WDMA2, NULL, cmucal_vclk_ip_qe_csis_wdma2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_WDMA3, NULL, cmucal_vclk_ip_qe_csis_wdma3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_D0, NULL, cmucal_vclk_ip_qe_pdp_d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_D1, NULL, cmucal_vclk_ip_qe_pdp_d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_D2, NULL, cmucal_vclk_ip_qe_pdp_d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_D3, NULL, cmucal_vclk_ip_qe_pdp_d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_CSIS, NULL, cmucal_vclk_ip_sysmmu_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_CSIS, NULL, cmucal_vclk_ip_sysmmu_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CSIS, NULL, cmucal_vclk_ip_sysreg_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D3_CSIS, NULL, cmucal_vclk_ip_xiu_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D4_CSIS, NULL, cmucal_vclk_ip_xiu_d4_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MIPI_PHY, NULL, cmucal_vclk_ip_ad_apb_mipi_phy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF0_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_si_otf0_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF1_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_si_otf1_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF2_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_si_otf2_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF3_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_si_otf3_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D3, NULL, cmucal_vclk_ip_ppmu_d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_WDMA4, NULL, cmucal_vclk_ip_qe_csis_wdma4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D3_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CSISPERIC1, NULL, cmucal_vclk_ip_slh_axi_si_p_csisperic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D3_CSIS, NULL, cmucal_vclk_ip_sysmmu_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D4, NULL, cmucal_vclk_ip_ppmu_d4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D4_CSIS, NULL, cmucal_vclk_ip_sysmmu_d4_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D0, NULL, cmucal_vclk_ip_vgen_lite_d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D1, NULL, cmucal_vclk_ip_vgen_lite_d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D4_CSIS, NULL, cmucal_vclk_ip_lh_axi_si_d4_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_CSIS, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G_PPMU_CSIS, NULL, cmucal_vclk_ip_siu_g_ppmu_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_SI_OTF_CSISDNC, NULL, cmucal_vclk_ip_slh_ast_si_otf_csisdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_SI_OTF_CSISUFD, NULL, cmucal_vclk_ip_slh_ast_si_otf_csisufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LP_UFDCSIS, NULL, cmucal_vclk_ip_slh_axi_mi_lp_ufdcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CSIS, NULL, cmucal_vclk_ip_slh_axi_mi_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSTAT_CMU_CSTAT, NULL, cmucal_vclk_ip_cstat_cmu_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CSTAT, NULL, cmucal_vclk_ip_d_tzpc_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSTAT0, NULL, cmucal_vclk_ip_ad_apb_cstat0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CSTAT, NULL, cmucal_vclk_ip_slh_axi_mi_p_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CSTAT, NULL, cmucal_vclk_ip_ppmu_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_CSTAT, NULL, cmucal_vclk_ip_sysmmu_d_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_CSTAT, NULL, cmucal_vclk_ip_lh_axi_si_d_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CSTAT, NULL, cmucal_vclk_ip_sysreg_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_CSTAT, NULL, cmucal_vclk_ip_xiu_d_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF0_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_mi_otf0_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF1_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_mi_otf1_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF2_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_mi_otf2_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF3_CSISCSTAT, NULL, cmucal_vclk_ip_lh_ast_mi_otf3_csiscstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIPU_CSTAT, NULL, cmucal_vclk_ip_sipu_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_CSTAT0, NULL, cmucal_vclk_ip_vgen_lite_cstat0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_CSTAT1, NULL, cmucal_vclk_ip_vgen_lite_cstat1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_CSTAT2, NULL, cmucal_vclk_ip_vgen_lite_cstat2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_CSTAT, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX_DBGCORE, NULL, cmucal_vclk_ip_ahb_busmatrix_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION_DBGCORE, NULL, cmucal_vclk_ip_grebeintegration_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_IP_APM, NULL, cmucal_vclk_ip_slh_axi_mi_ip_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_G_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_si_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_slh_axi_si_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_ID_DBGCORE, NULL, cmucal_vclk_ip_slh_axi_si_id_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DBGCORE, NULL, cmucal_vclk_ip_sysreg_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_DBGCORE, NULL, cmucal_vclk_ip_wdt_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XHB_DBGCORE, NULL, cmucal_vclk_ip_xhb_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DBGCORE_CMU_DBGCORE, NULL, cmucal_vclk_ip_dbgcore_cmu_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MDIS_DBGCORE, NULL, cmucal_vclk_ip_mdis_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DBGCORE_CORE, NULL, cmucal_vclk_ip_sysreg_dbgcore_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DBGCORE, NULL, cmucal_vclk_ip_d_tzpc_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_S2D_DBGCORE, NULL, cmucal_vclk_ip_apbif_s2d_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAHBMASTER_DBGCORE, NULL, cmucal_vclk_ip_asyncahbmaster_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_DSP0DNC_SHMEM, NULL, cmucal_vclk_ip_lh_axi_mi_ld_dsp0dnc_shmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_DSP1DNC_SHMEM, NULL, cmucal_vclk_ip_lh_axi_mi_ld_dsp1dnc_shmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_GNPU0DNC_SHMEM, NULL, cmucal_vclk_ip_lh_axi_mi_ld_gnpu0dnc_shmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_GNPU1DNC_SHMEM, NULL, cmucal_vclk_ip_lh_axi_mi_ld_gnpu1dnc_shmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNC_CMU_DNC, NULL, cmucal_vclk_ip_dnc_cmu_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_CMDQ_GNPU0, NULL, cmucal_vclk_ip_slh_axi_mi_d_cmdq_gnpu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_DSP0DNC_SFR, NULL, cmucal_vclk_ip_lh_axi_mi_ld_dsp0dnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_DSP1DNC_SFR, NULL, cmucal_vclk_ip_lh_axi_mi_ld_dsp1dnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_CMDQ_GNPU1, NULL, cmucal_vclk_ip_slh_axi_mi_d_cmdq_gnpu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_DNC, NULL, cmucal_vclk_ip_slh_axi_mi_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_DSP1DNC_CACHE, NULL, cmucal_vclk_ip_slh_axi_mi_ld_dsp1dnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_GNPU0, NULL, cmucal_vclk_ip_lh_axi_si_d1_gnpu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LP_IPDNC, NULL, cmucal_vclk_ip_lh_axi_mi_lp_ipdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_GNPU1, NULL, cmucal_vclk_ip_lh_axi_si_d1_gnpu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LP_IPDNC, NULL, cmucal_vclk_ip_lh_axi_si_lp_ipdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_MMU3, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_MMU2, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_MMU1, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_DNC, NULL, cmucal_vclk_ip_adm_dap_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_MMU0, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_mmu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_RQ_GNPU0, NULL, cmucal_vclk_ip_lh_axi_mi_d_rq_gnpu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_RQ_GNPU1, NULL, cmucal_vclk_ip_lh_axi_mi_d_rq_gnpu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_DNCDSP1_DMA, NULL, cmucal_vclk_ip_lh_axi_si_ld_dncdsp1_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_DSP0DNC_CACHE, NULL, cmucal_vclk_ip_slh_axi_mi_ld_dsp0dnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_GNPU0, NULL, cmucal_vclk_ip_lh_axi_si_d0_gnpu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_DNCDSP1_SFR, NULL, cmucal_vclk_ip_lh_axi_si_ld_dncdsp1_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_GNPU1, NULL, cmucal_vclk_ip_lh_axi_si_d0_gnpu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_DNC, NULL, cmucal_vclk_ip_ip_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_DNCDSP0_DMA, NULL, cmucal_vclk_ip_lh_axi_si_ld_dncdsp0_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DNC, NULL, cmucal_vclk_ip_sysreg_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_DNCDSP0_SFR, NULL, cmucal_vclk_ip_lh_axi_si_ld_dncdsp0_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DNC, NULL, cmucal_vclk_ip_d_tzpc_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA3, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA2, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA1, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA0, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA7, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data7, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_CTRL_GNPU1, NULL, cmucal_vclk_ip_lh_axi_si_d_ctrl_gnpu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA6, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_CTRL_GNPU0, NULL, cmucal_vclk_ip_lh_axi_si_d_ctrl_gnpu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA5, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_SDMADNC_DATA4, NULL, cmucal_vclk_ip_lh_axi_mi_ld_sdmadnc_data4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_IPDNC, NULL, cmucal_vclk_ip_ppmu_ipdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_SDMA0, NULL, cmucal_vclk_ip_ppmu_sdma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_SDMA1, NULL, cmucal_vclk_ip_ppmu_sdma1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_SDMA2, NULL, cmucal_vclk_ip_ppmu_sdma2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_SDMA3, NULL, cmucal_vclk_ip_ppmu_sdma3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_IPDNC, NULL, cmucal_vclk_ip_sysmmu_ipdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_SDMA0, NULL, cmucal_vclk_ip_sysmmu_sdma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_SDMA1, NULL, cmucal_vclk_ip_sysmmu_sdma1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_SDMA2, NULL, cmucal_vclk_ip_sysmmu_sdma2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_SDMA3, NULL, cmucal_vclk_ip_sysmmu_sdma3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_DNC, NULL, cmucal_vclk_ip_trex_d_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBAS_S1_NS_SDMA0, NULL, cmucal_vclk_ip_apbas_s1_ns_sdma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_DNC, NULL, cmucal_vclk_ip_busif_hpm_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_DNC, NULL, cmucal_vclk_ip_hpm_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_DNC, NULL, cmucal_vclk_ip_htu_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_DNC, NULL, cmucal_vclk_ip_busif_ddd_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_DNC, NULL, cmucal_vclk_ip_busif_add_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_DNC, NULL, cmucal_vclk_ip_add_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LD_DNCVTS, NULL, cmucal_vclk_ip_slh_axi_si_ld_dncvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LD_DNCCHUB, NULL, cmucal_vclk_ip_slh_axi_si_ld_dncchub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_DNCCHUB, NULL, cmucal_vclk_ip_baaw_dncchub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_DNCVTS, NULL, cmucal_vclk_ip_baaw_dncvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_DNC, NULL, cmucal_vclk_ip_ddd_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LP_DNCSDMA, NULL, cmucal_vclk_ip_lh_axi_si_lp_dncsdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G_PPMU_DNC, NULL, cmucal_vclk_ip_siu_g_ppmu_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_DNC, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_GNPU0, NULL, cmucal_vclk_ip_slh_axi_si_p_gnpu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_GNPU1, NULL, cmucal_vclk_ip_slh_axi_si_p_gnpu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_ALIVEDNC, NULL, cmucal_vclk_ip_slh_axi_mi_p_alivednc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_DNC, NULL, cmucal_vclk_ip_vgen_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_DNC, NULL, cmucal_vclk_ip_vgen_lite_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_GNPUP0, NULL, cmucal_vclk_ip_slh_axi_si_p_gnpup0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_GNPUP1, NULL, cmucal_vclk_ip_slh_axi_si_p_gnpup1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_UFDDNC, NULL, cmucal_vclk_ip_slh_axi_mi_ld_ufddnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_DNC, NULL, cmucal_vclk_ip_xiu_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_MI_OTF_CSISDNC, NULL, cmucal_vclk_ip_slh_ast_mi_otf_csisdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_GLUE_OTF_CSISDNC, NULL, cmucal_vclk_ip_slh_ast_glue_otf_csisdnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUB_CMU_DPUB, NULL, cmucal_vclk_ip_dpub_cmu_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DECON_MAIN, NULL, cmucal_vclk_ip_ad_apb_decon_main, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUB, NULL, cmucal_vclk_ip_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPUB, NULL, cmucal_vclk_ip_d_tzpc_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_DPUB, NULL, cmucal_vclk_ip_slh_axi_mi_p_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPUB, NULL, cmucal_vclk_ip_sysreg_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UPI_M0, NULL, cmucal_vclk_ip_upi_m0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF0_CMU_DPUF0, NULL, cmucal_vclk_ip_dpuf0_cmu_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DPUF0_DMA, NULL, cmucal_vclk_ip_ad_apb_dpuf0_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF0, NULL, cmucal_vclk_ip_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPUF0, NULL, cmucal_vclk_ip_d_tzpc_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_DPUF0, NULL, cmucal_vclk_ip_slh_axi_mi_p_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_DPUF0, NULL, cmucal_vclk_ip_lh_axi_si_d1_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF0D0, NULL, cmucal_vclk_ip_ppmu_dpuf0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF0D1, NULL, cmucal_vclk_ip_ppmu_dpuf0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF0D0, NULL, cmucal_vclk_ip_sysmmu_dpuf0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF0D1, NULL, cmucal_vclk_ip_sysmmu_dpuf0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPUF0, NULL, cmucal_vclk_ip_sysreg_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_DPUF0, NULL, cmucal_vclk_ip_xiu_d0_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_DPUF0, NULL, cmucal_vclk_ip_xiu_d1_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_DPUF0, NULL, cmucal_vclk_ip_siu_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_DPUF1DPUF0, NULL, cmucal_vclk_ip_lh_axi_mi_d0_dpuf1dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_DPUF1DPUF0, NULL, cmucal_vclk_ip_lh_axi_mi_d1_dpuf1dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_DPUF0, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D0_DPUF0, NULL, cmucal_vclk_ip_slh_axi_si_d0_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF1_CMU_DPUF1, NULL, cmucal_vclk_ip_dpuf1_cmu_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DPUF1_DMA, NULL, cmucal_vclk_ip_ad_apb_dpuf1_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF1, NULL, cmucal_vclk_ip_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPUF1, NULL, cmucal_vclk_ip_d_tzpc_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_DPUF1, NULL, cmucal_vclk_ip_slh_axi_mi_p_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF1D0, NULL, cmucal_vclk_ip_ppmu_dpuf1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF1D1, NULL, cmucal_vclk_ip_ppmu_dpuf1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF1D0, NULL, cmucal_vclk_ip_sysmmu_dpuf1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF1D1, NULL, cmucal_vclk_ip_sysmmu_dpuf1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPUF1, NULL, cmucal_vclk_ip_sysreg_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_DPUF1, NULL, cmucal_vclk_ip_siu_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_DPUF1DPUF0, NULL, cmucal_vclk_ip_lh_axi_si_d0_dpuf1dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_DPUF1DPUF0, NULL, cmucal_vclk_ip_lh_axi_si_d1_dpuf1dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_DPUF1, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DRCP_CMU_DRCP, NULL, cmucal_vclk_ip_drcp_cmu_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DRCP, NULL, cmucal_vclk_ip_ad_apb_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DRCP, NULL, cmucal_vclk_ip_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_DRCP, NULL, cmucal_vclk_ip_ppmu_d_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_DRCP, NULL, cmucal_vclk_ip_sysmmu_d_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DRCP, NULL, cmucal_vclk_ip_d_tzpc_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_DRCP, NULL, cmucal_vclk_ip_xiu_d_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DRCP, NULL, cmucal_vclk_ip_sysreg_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_DRCP, NULL, cmucal_vclk_ip_slh_axi_mi_p_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF_YUVPDRCP, NULL, cmucal_vclk_ip_lh_ast_mi_otf_yuvpdrcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF_DRCPMCSC, NULL, cmucal_vclk_ip_lh_ast_si_otf_drcpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_DRCP, NULL, cmucal_vclk_ip_lh_axi_si_d_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D_DRCP, NULL, cmucal_vclk_ip_vgen_lite_d_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_DRCP, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_DSPDNC_SHMEM, NULL, cmucal_vclk_ip_lh_axi_si_ld_dspdnc_shmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSP_CMU_DSP, NULL, cmucal_vclk_ip_dsp_cmu_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DSP, NULL, cmucal_vclk_ip_sysreg_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DSP, NULL, cmucal_vclk_ip_d_tzpc_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_DSP, NULL, cmucal_vclk_ip_slh_axi_mi_p_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_DNCDSP_DMA, NULL, cmucal_vclk_ip_lh_axi_mi_ld_dncdsp_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LD_DNCDSP_SFR, NULL, cmucal_vclk_ip_lh_axi_mi_ld_dncdsp_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LD_DSPDNC_CACHE, NULL, cmucal_vclk_ip_slh_axi_si_ld_dspdnc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_DSPDNC_SFR, NULL, cmucal_vclk_ip_lh_axi_si_ld_dspdnc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_DSP, NULL, cmucal_vclk_ip_ip_dsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_STRM_SDMADSP, NULL, cmucal_vclk_ip_lh_ast_mi_ld_strm_sdmadsp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLUSTER0, NULL, cmucal_vclk_ip_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_3, NULL, cmucal_vclk_ip_busif_str_cpucl0_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_IRI_GICCPU_CLUSTER0, NULL, cmucal_vclk_ip_lh_ast_mi_ld_iri_giccpu_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_ICC_CPUGIC_CLUSTER0, NULL, cmucal_vclk_ip_lh_ast_si_ld_icc_cpugic_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_SI_IT_CLUSTER0, NULL, cmucal_vclk_ip_lh_atb_si_it_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRET_CLUSTER0_0, NULL, cmucal_vclk_ip_ppc_instrret_cluster0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRET_CLUSTER0_1, NULL, cmucal_vclk_ip_ppc_instrret_cluster0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRUN_CLUSTER0_0, NULL, cmucal_vclk_ip_ppc_instrrun_cluster0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRUN_CLUSTER0_1, NULL, cmucal_vclk_ip_ppc_instrrun_cluster0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_PPU_ALIVE_CPUCL0, NULL, cmucal_vclk_ip_slh_axi_mi_ppu_alive_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSU_CMU_DSU, NULL, cmucal_vclk_ip_dsu_cmu_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_DSU, NULL, cmucal_vclk_ip_htu_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_CHI_SI_D0_CLUSTER0, NULL, cmucal_vclk_ip_lh_chi_si_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_CHI_SI_D1_CLUSTER0, NULL, cmucal_vclk_ip_lh_chi_si_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_CNTVALUEB, NULL, cmucal_vclk_ip_gray2bin_cntvalueb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_U_SYNC_PPUWAKEUP_CLUSTER0, NULL, cmucal_vclk_ip_u_sync_ppuwakeup_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_IP_UTILITY, NULL, cmucal_vclk_ip_slh_axi_mi_ip_utility, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_UTILITY, NULL, cmucal_vclk_ip_xiu_dp_utility, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CLUSTER0_NOCL0, NULL, cmucal_vclk_ip_slh_axi_si_p_cluster0_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_IP_UTILITY, NULL, cmucal_vclk_ip_slh_axi_si_ip_utility, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_PERIPHERAL, NULL, cmucal_vclk_ip_xiu_dp_peripheral, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_STR_CPUCL0_3, NULL, cmucal_vclk_ip_str_cpucl0_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMG3D, NULL, cmucal_vclk_ip_busif_hpmg3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_INT_G3D, NULL, cmucal_vclk_ip_slh_axi_si_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_G3D, NULL, cmucal_vclk_ip_hpm_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G3D, NULL, cmucal_vclk_ip_d_tzpc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_G3D, NULL, cmucal_vclk_ip_slh_axi_mi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CFM_G3D, NULL, cmucal_vclk_ip_cfm_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BG3D_PWRCTL, NULL, cmucal_vclk_ip_bg3d_pwrctl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_G3D, NULL, cmucal_vclk_ip_add_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_APBIF_G3D, NULL, cmucal_vclk_ip_add_apbif_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_G3D, NULL, cmucal_vclk_ip_busif_str_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_G3D, NULL, cmucal_vclk_ip_htu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3DCORE_CMU_G3DCORE, NULL, cmucal_vclk_ip_g3dcore_cmu_g3dcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPU, NULL, cmucal_vclk_ip_gpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_G_G3D, NULL, cmucal_vclk_ip_adm_dap_g_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDC_G3D, NULL, cmucal_vclk_ip_busif_ddc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_STR_G3D, NULL, cmucal_vclk_ip_str_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GNPU_CMU_GNPU, NULL, cmucal_vclk_ip_gnpu_cmu_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUCORE, NULL, cmucal_vclk_ip_ip_npucore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_CTRL_GNPU, NULL, cmucal_vclk_ip_lh_axi_mi_d_ctrl_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_RQ_GNPU, NULL, cmucal_vclk_ip_lh_axi_si_d_rq_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU0, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU31, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au31, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU30, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au30, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU4, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU3, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU2, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU1, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU8, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au8, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU7, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au7, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU6, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU5, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU9, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au9, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_GNPU, NULL, cmucal_vclk_ip_lh_axi_mi_d1_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU20, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au20, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU22, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au22, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU21, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au21, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU24, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au24, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU23, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au23, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU26, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au26, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU25, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au25, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU28, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au28, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU27, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au27, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU29, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au29, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_GNPU, NULL, cmucal_vclk_ip_d_tzpc_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPUP_BU3, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPUP_BU2, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_CU_RDATA, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_cu_rdata, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AULOADER, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_auloader, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPUP_RDREQ, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpup_rdreq, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPUP_CU_WCH, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpup_cu_wch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU11, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au11, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPUP_BU1, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU10, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPUP_BU0, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpup_bu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU13, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au13, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU12, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au12, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_GNPU, NULL, cmucal_vclk_ip_slh_axi_mi_p_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_CMDQ_GNPU, NULL, cmucal_vclk_ip_slh_axi_si_d_cmdq_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU15, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au15, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU14, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au14, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU17, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au17, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU16, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au16, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU19, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au19, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_AU18, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_au18, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_GNPU, NULL, cmucal_vclk_ip_sysreg_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_GNPU, NULL, cmucal_vclk_ip_lh_axi_mi_d0_gnpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_NPUC_SETREG, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_npuc_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPUP_NPUC_DONE, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpup_npuc_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_GNPUDNC_SHMEM, NULL, cmucal_vclk_ip_lh_axi_si_ld_gnpudnc_shmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_GNPU_0, NULL, cmucal_vclk_ip_xiu_d_gnpu_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_GNPU_1, NULL, cmucal_vclk_ip_xiu_d_gnpu_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPU_SRAM_WRRESP, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpu_sram_wrresp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GNPUP_CMU_GNPUP, NULL, cmucal_vclk_ip_gnpup_cmu_gnpup, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU3, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_CU_RDATA, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_cu_rdata, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU4, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU5, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU6, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPUP_BU1, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU7, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au7, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPUP_BU2, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU8, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au8, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_GNPUP, NULL, cmucal_vclk_ip_d_tzpc_gnpup, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPUP_BU3, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU9, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au9, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_GNPUP, NULL, cmucal_vclk_ip_sysreg_gnpup, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPUP_BU0, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpup_bu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPUP_RDREQ, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpup_rdreq, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU17, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au17, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU16, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au16, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU19, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au19, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU18, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au18, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU13, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au13, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU12, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au12, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU15, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au15, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU14, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au14, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU31, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au31, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU0, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU30, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au30, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU1, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU11, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au11, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU2, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU10, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_NPUC_SETREG, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_npuc_setreg, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AULOADER, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_auloader, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU28, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au28, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU27, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au27, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU29, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au29, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU24, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au24, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPUP_NPUC_DONE, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpup_npuc_done, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU23, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au23, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU26, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au26, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_GNPUP, NULL, cmucal_vclk_ip_slh_axi_mi_p_gnpup, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU25, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au25, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUPOST, NULL, cmucal_vclk_ip_ip_npupost, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU20, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au20, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_GNPUP_CU_WCH, NULL, cmucal_vclk_ip_lh_ast_si_ld_gnpup_cu_wch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU22, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au22, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_AU21, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_au21, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_GNPU_SRAM_WRRESP, NULL, cmucal_vclk_ip_lh_ast_mi_ld_gnpu_sram_wrresp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GNSS_CMU_GNSS, NULL, cmucal_vclk_ip_gnss_cmu_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_HSI0, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DP_LINK, NULL, cmucal_vclk_ip_dp_link, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI0_CMU_HSI0, NULL, cmucal_vclk_ip_hsi0_cmu_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI0, NULL, cmucal_vclk_ip_d_tzpc_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_HSI0, NULL, cmucal_vclk_ip_slh_axi_mi_p_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ACEL_SI_D_HSI0, NULL, cmucal_vclk_ip_slh_acel_si_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LD_HSI0AUD, NULL, cmucal_vclk_ip_slh_axi_si_ld_hsi0aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI0_BUS1, NULL, cmucal_vclk_ip_ppmu_hsi0_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_HSI0, NULL, cmucal_vclk_ip_sysmmu_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI0, NULL, cmucal_vclk_ip_sysreg_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB32DRD, NULL, cmucal_vclk_ip_usb32drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI0, NULL, cmucal_vclk_ip_vgen_lite_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_HSI0, NULL, cmucal_vclk_ip_xiu_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_AUDHSI0, NULL, cmucal_vclk_ip_slh_axi_mi_ld_audhsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPC_HSI0, NULL, cmucal_vclk_ip_spc_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_URAM, NULL, cmucal_vclk_ip_uram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P0_HSI0, NULL, cmucal_vclk_ip_xiu_p0_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_EUSBPHY_HSI0, NULL, cmucal_vclk_ip_as_apb_eusbphy_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_GEN2, NULL, cmucal_vclk_ip_pcie_gen2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI1, NULL, cmucal_vclk_ip_d_tzpc_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI1, NULL, cmucal_vclk_ip_gpio_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_HSI1, NULL, cmucal_vclk_ip_slh_axi_mi_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_SI_D_HSI1, NULL, cmucal_vclk_ip_lh_acel_si_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_GEN3, NULL, cmucal_vclk_ip_pcie_gen3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_IA_GEN3, NULL, cmucal_vclk_ip_pcie_ia_gen3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI1, NULL, cmucal_vclk_ip_ppmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_HSI1, NULL, cmucal_vclk_ip_sysmmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI1, NULL, cmucal_vclk_ip_sysreg_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFS_EMBD, NULL, cmucal_vclk_ip_ufs_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI1, NULL, cmucal_vclk_ip_vgen_lite_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_HSI1, NULL, cmucal_vclk_ip_xiu_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_HSI1, NULL, cmucal_vclk_ip_xiu_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI1_CMU_HSI1, NULL, cmucal_vclk_ip_hsi1_cmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_IA_GEN2, NULL, cmucal_vclk_ip_pcie_ia_gen2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI1UFS, NULL, cmucal_vclk_ip_gpio_hsi1ufs, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPC_HSI1, NULL, cmucal_vclk_ip_spc_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_HSI1, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LME_CMU_LME, NULL, cmucal_vclk_ip_lme_cmu_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D_LME, NULL, cmucal_vclk_ip_lh_axi_si_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_LME, NULL, cmucal_vclk_ip_ppmu_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_FRC_MC, NULL, cmucal_vclk_ip_frc_mc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_LME, NULL, cmucal_vclk_ip_ad_apb_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_FRC_MC, NULL, cmucal_vclk_ip_ad_axi_frc_mc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_LME, NULL, cmucal_vclk_ip_slh_axi_mi_p_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_LME, NULL, cmucal_vclk_ip_sysreg_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_LME, NULL, cmucal_vclk_ip_d_tzpc_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LME, NULL, cmucal_vclk_ip_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_FRC_MC, NULL, cmucal_vclk_ip_ad_apb_frc_mc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_LME, NULL, cmucal_vclk_ip_sysmmu_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_LME, NULL, cmucal_vclk_ip_xiu_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D0_LME, NULL, cmucal_vclk_ip_qe_d0_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D1_LME, NULL, cmucal_vclk_ip_qe_d1_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D_LME, NULL, cmucal_vclk_ip_vgen_lite_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_LME, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M_CMU_M2M, NULL, cmucal_vclk_ip_m2m_cmu_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_M2M, NULL, cmucal_vclk_ip_as_apb_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_M2M, NULL, cmucal_vclk_ip_d_tzpc_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_M2M, NULL, cmucal_vclk_ip_slh_axi_mi_p_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_SI_D_M2M, NULL, cmucal_vclk_ip_lh_acel_si_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M, NULL, cmucal_vclk_ip_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_M2M, NULL, cmucal_vclk_ip_ppmu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ASTC, NULL, cmucal_vclk_ip_qe_astc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JPEG0, NULL, cmucal_vclk_ip_qe_jpeg0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JPEG1, NULL, cmucal_vclk_ip_qe_jpeg1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JSQZ, NULL, cmucal_vclk_ip_qe_jsqz, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_M2M, NULL, cmucal_vclk_ip_qe_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_M2M_PM, NULL, cmucal_vclk_ip_sysmmu_d_m2m_pm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_M2M, NULL, cmucal_vclk_ip_sysreg_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_M2M, NULL, cmucal_vclk_ip_xiu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASTC, NULL, cmucal_vclk_ip_astc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG0, NULL, cmucal_vclk_ip_jpeg0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG1, NULL, cmucal_vclk_ip_jpeg1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JSQZ, NULL, cmucal_vclk_ip_jsqz, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_M2M, NULL, cmucal_vclk_ip_vgen_lite_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_M2M, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCFP_CMU_MCFP, NULL, cmucal_vclk_ip_mcfp_cmu_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCFP, NULL, cmucal_vclk_ip_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D2_MCFP, NULL, cmucal_vclk_ip_lh_axi_si_d2_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D5_MCFP, NULL, cmucal_vclk_ip_ppmu_d5_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D2_MCFP, NULL, cmucal_vclk_ip_ppmu_d2_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_MCFP, NULL, cmucal_vclk_ip_xiu_d1_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D5_MCFP, NULL, cmucal_vclk_ip_slh_axi_si_d5_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MCFP, NULL, cmucal_vclk_ip_lh_axi_si_d1_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D4_MCFP, NULL, cmucal_vclk_ip_slh_axi_si_d4_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_MCFP, NULL, cmucal_vclk_ip_xiu_d0_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_MCFP, NULL, cmucal_vclk_ip_ppmu_d1_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D3_MCFP, NULL, cmucal_vclk_ip_slh_axi_si_d3_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D4_MCFP, NULL, cmucal_vclk_ip_ppmu_d4_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_MCFP, NULL, cmucal_vclk_ip_lh_axi_si_d0_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCFP, NULL, cmucal_vclk_ip_sysreg_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_MCFP, NULL, cmucal_vclk_ip_sysmmu_d0_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_MCFP, NULL, cmucal_vclk_ip_sysmmu_d2_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCFP, NULL, cmucal_vclk_ip_d_tzpc_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_MCFP, NULL, cmucal_vclk_ip_sysmmu_d1_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D4_MCFP, NULL, cmucal_vclk_ip_sysmmu_d4_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D3_MCFP, NULL, cmucal_vclk_ip_ppmu_d3_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D3_MCFP, NULL, cmucal_vclk_ip_sysmmu_d3_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCFP, NULL, cmucal_vclk_ip_ad_apb_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_MCFP, NULL, cmucal_vclk_ip_ppmu_d0_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D5_MCFP, NULL, cmucal_vclk_ip_sysmmu_d5_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MCFP, NULL, cmucal_vclk_ip_slh_axi_mi_p_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF_RGBPMCFP, NULL, cmucal_vclk_ip_lh_ast_mi_otf_rgbpmcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF0_MCFPYUVP, NULL, cmucal_vclk_ip_lh_ast_si_otf0_mcfpyuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF_YUVPMCFP, NULL, cmucal_vclk_ip_lh_ast_mi_otf_yuvpmcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D0_MCFP, NULL, cmucal_vclk_ip_vgen_lite_d0_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D1_MCFP, NULL, cmucal_vclk_ip_vgen_lite_d1_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_MCFP, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G_PPMU_MCFP, NULL, cmucal_vclk_ip_siu_g_ppmu_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC_CMU_MCSC, NULL, cmucal_vclk_ip_mcsc_cmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_GDC, NULL, cmucal_vclk_ip_ad_apb_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCSC, NULL, cmucal_vclk_ip_ad_apb_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCSC, NULL, cmucal_vclk_ip_d_tzpc_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GDC, NULL, cmucal_vclk_ip_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF_DRCPMCSC, NULL, cmucal_vclk_ip_lh_ast_mi_otf_drcpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MCSC, NULL, cmucal_vclk_ip_slh_axi_mi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_SI_D0_MCSC, NULL, cmucal_vclk_ip_lh_acel_si_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MCSC, NULL, cmucal_vclk_ip_lh_axi_si_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D2_MCSC, NULL, cmucal_vclk_ip_lh_axi_si_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC, NULL, cmucal_vclk_ip_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_MCSC, NULL, cmucal_vclk_ip_ppmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_MCSC, NULL, cmucal_vclk_ip_ppmu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D2_MCSC, NULL, cmucal_vclk_ip_ppmu_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_MCSC, NULL, cmucal_vclk_ip_sysmmu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_MCSC, NULL, cmucal_vclk_ip_sysmmu_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCSC, NULL, cmucal_vclk_ip_sysreg_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_MCSC, NULL, cmucal_vclk_ip_xiu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_MCSC, NULL, cmucal_vclk_ip_xiu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D2_MCSC, NULL, cmucal_vclk_ip_xiu_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_MCSC, NULL, cmucal_vclk_ip_sysmmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D0_MCSC, NULL, cmucal_vclk_ip_vgen_lite_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D1_MCSC, NULL, cmucal_vclk_ip_vgen_lite_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_MCSC, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G_PPMU_MCSC, NULL, cmucal_vclk_ip_siu_g_ppmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_IG_PPMU_D0_MCSC, NULL, cmucal_vclk_ip_slh_astl_mi_ig_ppmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_IG_PPMU_D0_MCSC, NULL, cmucal_vclk_ip_slh_astl_si_ig_ppmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC0_CMU_MFC0, NULL, cmucal_vclk_ip_mfc0_cmu_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MFC0, NULL, cmucal_vclk_ip_xiu_d_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WFD, NULL, cmucal_vclk_ip_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC0D0, NULL, cmucal_vclk_ip_sysmmu_mfc0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_WFD, NULL, cmucal_vclk_ip_ppmu_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC0D1, NULL, cmucal_vclk_ip_ppmu_mfc0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC0D0, NULL, cmucal_vclk_ip_ppmu_mfc0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC0, NULL, cmucal_vclk_ip_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MFC0, NULL, cmucal_vclk_ip_lh_atb_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MFC0, NULL, cmucal_vclk_ip_lh_axi_si_d1_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_MFC0, NULL, cmucal_vclk_ip_lh_axi_si_d0_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF2_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_si_otf2_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF1_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_si_otf1_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF0_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_si_otf0_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MFC0, NULL, cmucal_vclk_ip_slh_axi_mi_p_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF3_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_mi_otf3_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF2_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_mi_otf2_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF1_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_mi_otf1_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF0_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_mi_otf0_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFC0, NULL, cmucal_vclk_ip_d_tzpc_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_WFD, NULL, cmucal_vclk_ip_as_axi_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_APB_MFC0MFC1, NULL, cmucal_vclk_ip_ads_apb_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC0, NULL, cmucal_vclk_ip_as_apb_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_WFD_NS, NULL, cmucal_vclk_ip_as_apb_wfd_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC0, NULL, cmucal_vclk_ip_sysreg_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF3_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_si_otf3_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC0D1, NULL, cmucal_vclk_ip_sysmmu_mfc0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_MFC0, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G_PPMU_MFC0, NULL, cmucal_vclk_ip_siu_g_ppmu_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MFC0, NULL, cmucal_vclk_ip_vgen_lite_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC1_CMU_MFC1, NULL, cmucal_vclk_ip_mfc1_cmu_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC1, NULL, cmucal_vclk_ip_sysreg_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC1D0, NULL, cmucal_vclk_ip_sysmmu_mfc1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF0_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_mi_otf0_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFC1, NULL, cmucal_vclk_ip_d_tzpc_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC1, NULL, cmucal_vclk_ip_as_apb_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_MFC0MFC1, NULL, cmucal_vclk_ip_adm_apb_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF1_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_mi_otf1_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF2_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_mi_otf2_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF3_MFC0MFC1, NULL, cmucal_vclk_ip_lh_ast_mi_otf3_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MFC1, NULL, cmucal_vclk_ip_slh_axi_mi_p_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF0_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_si_otf0_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF1_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_si_otf1_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF2_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_si_otf2_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF3_MFC1MFC0, NULL, cmucal_vclk_ip_lh_ast_si_otf3_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_MFC1, NULL, cmucal_vclk_ip_lh_axi_si_d0_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_MFC1, NULL, cmucal_vclk_ip_lh_axi_si_d1_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC1, NULL, cmucal_vclk_ip_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC1D0, NULL, cmucal_vclk_ip_ppmu_mfc1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC1D1, NULL, cmucal_vclk_ip_ppmu_mfc1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC1D1, NULL, cmucal_vclk_ip_sysmmu_mfc1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_MFC1, NULL, cmucal_vclk_ip_vgen_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_MFC1, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G_PPMU_MFC1, NULL, cmucal_vclk_ip_siu_g_ppmu_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DDRPHY, NULL, cmucal_vclk_ip_apbbr_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMC, NULL, cmucal_vclk_ip_apbbr_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY, NULL, cmucal_vclk_ip_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF, NULL, cmucal_vclk_ip_d_tzpc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_MIF, NULL, cmucal_vclk_ip_slh_axi_mi_p_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPC_DEBUG, NULL, cmucal_vclk_ip_qch_adapter_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_G_DMC, NULL, cmucal_vclk_ip_lh_ast_si_g_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPC_MIF, NULL, cmucal_vclk_ip_spc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PRIVATE_MIF, NULL, cmucal_vclk_ip_sysreg_private_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_MIF, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_MIF, NULL, cmucal_vclk_ip_busif_ddd_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_MIF, NULL, cmucal_vclk_ip_ddd_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG, NULL, cmucal_vclk_ip_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NOCL0_CMU_NOCL0, NULL, cmucal_vclk_ip_nocl0_cmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CCI, NULL, cmucal_vclk_ip_cci, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G0_PPMU_NOCL0, NULL, cmucal_vclk_ip_siu_g0_ppmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G1_PPMU_NOCL0, NULL, cmucal_vclk_ip_siu_g1_ppmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_SYSMMU_S2_G3D, NULL, cmucal_vclk_ip_apb_async_sysmmu_s2_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_ASB_CSSYS, NULL, cmucal_vclk_ip_axi_asb_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_CP, NULL, cmucal_vclk_ip_baaw_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BDU, NULL, cmucal_vclk_ip_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NOCL0, NULL, cmucal_vclk_ip_d_tzpc_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D2_MODEM, NULL, cmucal_vclk_ip_slh_axi_mi_d2_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_CHI_MI_D0_CLUSTER0, NULL, cmucal_vclk_ip_lh_chi_mi_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_D0_G3D, NULL, cmucal_vclk_ip_lh_acel_mi_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_CHI_MI_D1_CLUSTER0, NULL, cmucal_vclk_ip_lh_chi_mi_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_D1_G3D, NULL, cmucal_vclk_ip_lh_acel_mi_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_D2_G3D, NULL, cmucal_vclk_ip_lh_acel_mi_d2_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_D3_G3D, NULL, cmucal_vclk_ip_lh_acel_mi_d3_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D0_MODEM, NULL, cmucal_vclk_ip_slh_axi_mi_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D1_MODEM, NULL, cmucal_vclk_ip_slh_axi_mi_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_QDI_MI_D_AUD, NULL, cmucal_vclk_ip_lh_qdi_mi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_G_CSSYS, NULL, cmucal_vclk_ip_lh_axi_mi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_SI_T_BDU, NULL, cmucal_vclk_ip_lh_atb_si_t_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_APM, NULL, cmucal_vclk_ip_slh_axi_si_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_AUD, NULL, cmucal_vclk_ip_slh_axi_si_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CPUCL0, NULL, cmucal_vclk_ip_slh_axi_si_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_G3D, NULL, cmucal_vclk_ip_slh_axi_si_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MODEM, NULL, cmucal_vclk_ip_slh_axi_si_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_PERISGIC, NULL, cmucal_vclk_ip_slh_axi_si_p_perisgic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE2AXI_0, NULL, cmucal_vclk_ip_mpace2axi_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE2AXI_1, NULL, cmucal_vclk_ip_mpace2axi_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D0_MIF, NULL, cmucal_vclk_ip_mpace_asb_d0_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D1_MIF, NULL, cmucal_vclk_ip_mpace_asb_d1_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D2_MIF, NULL, cmucal_vclk_ip_mpace_asb_d2_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D3_MIF, NULL, cmucal_vclk_ip_mpace_asb_d3_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG_CCI, NULL, cmucal_vclk_ip_ppc_debug_cci, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL0_0, NULL, cmucal_vclk_ip_ppmu_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL0_1, NULL, cmucal_vclk_ip_ppmu_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D0, NULL, cmucal_vclk_ip_ppmu_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D1, NULL, cmucal_vclk_ip_ppmu_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D2, NULL, cmucal_vclk_ip_ppmu_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D3, NULL, cmucal_vclk_ip_ppmu_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF0, NULL, cmucal_vclk_ip_ppmu_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF1, NULL, cmucal_vclk_ip_ppmu_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF2, NULL, cmucal_vclk_ip_ppmu_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF3, NULL, cmucal_vclk_ip_ppmu_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_G3D, NULL, cmucal_vclk_ip_sysmmu_s2_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MODEM, NULL, cmucal_vclk_ip_sysmmu_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NOCL0, NULL, cmucal_vclk_ip_sysreg_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NOCL0, NULL, cmucal_vclk_ip_trex_d_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_NOCL0, NULL, cmucal_vclk_ip_trex_p_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MODEM, NULL, cmucal_vclk_ip_vgen_lite_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCSFR_WR_SMC, NULL, cmucal_vclk_ip_asyncsfr_wr_smc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MIF0, NULL, cmucal_vclk_ip_slh_axi_si_p_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MIF1, NULL, cmucal_vclk_ip_slh_axi_si_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MIF2, NULL, cmucal_vclk_ip_slh_axi_si_p_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MIF3, NULL, cmucal_vclk_ip_slh_axi_si_p_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_APM, NULL, cmucal_vclk_ip_sysmmu_s2_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_APM, NULL, cmucal_vclk_ip_lh_axi_mi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MCW, NULL, cmucal_vclk_ip_slh_axi_si_p_mcw, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CLUSTER0_NOCL0, NULL, cmucal_vclk_ip_slh_axi_mi_p_cluster0_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CACHEAID_NOCL0, NULL, cmucal_vclk_ip_cacheaid_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_APM, NULL, cmucal_vclk_ip_ppmu_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G0_DMC, NULL, cmucal_vclk_ip_lh_ast_mi_g0_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G1_DMC, NULL, cmucal_vclk_ip_lh_ast_mi_g1_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G2_DMC, NULL, cmucal_vclk_ip_lh_ast_mi_g2_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G3_DMC, NULL, cmucal_vclk_ip_lh_ast_mi_g3_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G_NOCL1A, NULL, cmucal_vclk_ip_lh_ast_mi_g_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G_NOCL1B, NULL, cmucal_vclk_ip_lh_ast_mi_g_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_G_NOCL2A, NULL, cmucal_vclk_ip_lh_ast_mi_g_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_D0_MIF, NULL, cmucal_vclk_ip_wow_dvfs_d0_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_IRPS0, NULL, cmucal_vclk_ip_wow_dvfs_irps0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_IRPS1, NULL, cmucal_vclk_ip_wow_dvfs_irps1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_IRPS2, NULL, cmucal_vclk_ip_wow_dvfs_irps2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_IRPS3, NULL, cmucal_vclk_ip_wow_dvfs_irps3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_NOCL0, NULL, cmucal_vclk_ip_wow_dvfs_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_SCI, NULL, cmucal_vclk_ip_ppc_sci, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPC_NOCL0, NULL, cmucal_vclk_ip_spc_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PRIVATE_NOCL0, NULL, cmucal_vclk_ip_sysreg_private_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_D0_CPUCL0, NULL, cmucal_vclk_ip_wow_dvfs_d0_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_D0_G3D, NULL, cmucal_vclk_ip_wow_dvfs_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_D1_CPUCL0, NULL, cmucal_vclk_ip_wow_dvfs_d1_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_AUD, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MODEM, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_NOCL1A, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_NOCL1B, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G6_PPMU_NOCL0, NULL, cmucal_vclk_ip_siu_g6_ppmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G2_PPMU_NOCL0, NULL, cmucal_vclk_ip_siu_g2_ppmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G3_PPMU_NOCL0, NULL, cmucal_vclk_ip_siu_g3_ppmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G4_PPMU_NOCL0, NULL, cmucal_vclk_ip_siu_g4_ppmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G5_PPMU_NOCL0, NULL, cmucal_vclk_ip_siu_g5_ppmu_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_ETR, NULL, cmucal_vclk_ip_apb_async_etr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ETR_64_NOCL0, NULL, cmucal_vclk_ip_etr_64_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MIF0, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MIF1, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MIF2, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MIF3, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_SYNC_GEN, NULL, cmucal_vclk_ip_ppmu_sync_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PBHA_GEN_D0_MODEM, NULL, cmucal_vclk_ip_pbha_gen_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PBHA_GEN_D1_MODEM, NULL, cmucal_vclk_ip_pbha_gen_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_D1_MIF, NULL, cmucal_vclk_ip_wow_dvfs_d1_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_D2_MIF, NULL, cmucal_vclk_ip_wow_dvfs_d2_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WOW_DVFS_D3_MIF, NULL, cmucal_vclk_ip_wow_dvfs_d3_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_D0_G3D, NULL, cmucal_vclk_ip_vgen_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_D1_G3D, NULL, cmucal_vclk_ip_vgen_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_D2_G3D, NULL, cmucal_vclk_ip_vgen_d2_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_D3_G3D, NULL, cmucal_vclk_ip_vgen_d3_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_IG_PPMU_P_CLUSTER0, NULL, cmucal_vclk_ip_slh_astl_si_ig_ppmu_p_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_IG_PPMU_P_CLUSTER0, NULL, cmucal_vclk_ip_slh_astl_mi_ig_ppmu_p_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_VGEN_D0_G3D, NULL, cmucal_vclk_ip_apb_async_vgen_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_VGEN_D1_G3D, NULL, cmucal_vclk_ip_apb_async_vgen_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_VGEN_D2_G3D, NULL, cmucal_vclk_ip_apb_async_vgen_d2_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_VGEN_D3_G3D, NULL, cmucal_vclk_ip_apb_async_vgen_d3_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_SI_IG_DEBUG_MUX_NOCL0, NULL, cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_MI_IG_DEBUG_MUX_NOCL0, NULL, cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NOCL1A_CMU_NOCL1A, NULL, cmucal_vclk_ip_nocl1a_cmu_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_SSP, NULL, cmucal_vclk_ip_slh_axi_si_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NOCL1A, NULL, cmucal_vclk_ip_sysreg_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NOCL1A, NULL, cmucal_vclk_ip_trex_d_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_HSI1, NULL, cmucal_vclk_ip_slh_axi_si_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MFC1, NULL, cmucal_vclk_ip_slh_axi_si_p_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_MFC1, NULL, cmucal_vclk_ip_lh_axi_mi_d1_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_G_NOCL1A, NULL, cmucal_vclk_ip_lh_ast_si_g_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_MFC0, NULL, cmucal_vclk_ip_lh_axi_mi_d1_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MFC0, NULL, cmucal_vclk_ip_slh_axi_si_p_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NOCIF_CMUTOPC, NULL, cmucal_vclk_ip_nocif_cmutopc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_D_M2M, NULL, cmucal_vclk_ip_lh_acel_mi_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NOCL1A, NULL, cmucal_vclk_ip_d_tzpc_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_LME, NULL, cmucal_vclk_ip_lh_axi_mi_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_M2M, NULL, cmucal_vclk_ip_slh_axi_si_p_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_NOCL1A, NULL, cmucal_vclk_ip_trex_p_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_MFC1, NULL, cmucal_vclk_ip_lh_axi_mi_d0_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_LME, NULL, cmucal_vclk_ip_slh_axi_si_p_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_MFC0, NULL, cmucal_vclk_ip_lh_axi_mi_d0_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_D_HSI1, NULL, cmucal_vclk_ip_lh_acel_mi_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ACEL_MI_D_SSP, NULL, cmucal_vclk_ip_slh_acel_mi_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_HSI1, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_LME, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_M2M, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MFC0, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MFC1, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_NOCL1A, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_NOCL2A, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_SSP, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_2X1_P0_NOCL1A, NULL, cmucal_vclk_ip_siu_2x1_p0_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_4X1_P0_NOCL1A, NULL, cmucal_vclk_ip_siu_4x1_p0_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_8X1_P0_NOCL1A, NULL, cmucal_vclk_ip_siu_8x1_p0_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_PERIC0, NULL, cmucal_vclk_ip_slh_axi_si_p_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_PERIC2, NULL, cmucal_vclk_ip_slh_axi_si_p_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_IG_PPMU_NOCL1A, NULL, cmucal_vclk_ip_slh_astl_mi_ig_ppmu_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_IG_PPMU_NOCL1A, NULL, cmucal_vclk_ip_slh_astl_si_ig_ppmu_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_MI_IG_DEBUG_MUX_NOCL1A, NULL, cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_SI_IG_DEBUG_MUX_NOCL1A, NULL, cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl1a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NOCL1B_CMU_NOCL1B, NULL, cmucal_vclk_ip_nocl1b_cmu_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_VTS, NULL, cmucal_vclk_ip_baaw_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NOCL1B, NULL, cmucal_vclk_ip_d_tzpc_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ACEL_MI_D_HSI0, NULL, cmucal_vclk_ip_slh_acel_mi_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D0_DPUF0, NULL, cmucal_vclk_ip_slh_axi_mi_d0_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_DPUF0, NULL, cmucal_vclk_ip_lh_axi_mi_d1_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DPUB, NULL, cmucal_vclk_ip_slh_axi_si_p_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DPUF0, NULL, cmucal_vclk_ip_slh_axi_si_p_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DPUF1, NULL, cmucal_vclk_ip_slh_axi_si_p_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_HSI0, NULL, cmucal_vclk_ip_slh_axi_si_p_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_VTS, NULL, cmucal_vclk_ip_slh_axi_si_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_G_NOCL1B, NULL, cmucal_vclk_ip_lh_ast_si_g_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDMA, NULL, cmucal_vclk_ip_qe_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_SPDMA, NULL, cmucal_vclk_ip_qe_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NOCL1B, NULL, cmucal_vclk_ip_sysreg_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NOCL1B, NULL, cmucal_vclk_ip_trex_d_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_NOCL1B, NULL, cmucal_vclk_ip_trex_p_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_RB_NOCL1B, NULL, cmucal_vclk_ip_trex_rb_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CHUB, NULL, cmucal_vclk_ip_slh_axi_si_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_GNSS, NULL, cmucal_vclk_ip_slh_axi_si_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_CHUB, NULL, cmucal_vclk_ip_baaw_p_chub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_GNSS, NULL, cmucal_vclk_ip_baaw_p_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_UFD, NULL, cmucal_vclk_ip_slh_axi_si_p_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DNC, NULL, cmucal_vclk_ip_slh_axi_si_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DSP0, NULL, cmucal_vclk_ip_slh_axi_si_p_dsp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DSP1, NULL, cmucal_vclk_ip_slh_axi_si_p_dsp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_PERIS, NULL, cmucal_vclk_ip_slh_axi_si_p_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_SDMA, NULL, cmucal_vclk_ip_slh_axi_si_p_sdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_TT, NULL, cmucal_vclk_ip_ppmu_d_tt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DIT, NULL, cmucal_vclk_ip_ppmu_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_DNC, NULL, cmucal_vclk_ip_baaw_p_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_HSI0, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_P0_TREX_XIU_DIT, NULL, cmucal_vclk_ip_siu_p0_trex_xiu_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_P1_HSI_DPUF_DNC_GNSS, NULL, cmucal_vclk_ip_siu_p1_hsi_dpuf_dnc_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_DPUF0, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_DPUF1, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_DNC, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_dnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_NOCL1B, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_GNSS, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_gnss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_PERIC1, NULL, cmucal_vclk_ip_slh_axi_si_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DIT, NULL, cmucal_vclk_ip_ad_apb_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA, NULL, cmucal_vclk_ip_ad_apb_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SPDMA, NULL, cmucal_vclk_ip_ad_apb_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_DIT, NULL, cmucal_vclk_ip_ad_apb_sysmmu_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_TT, NULL, cmucal_vclk_ip_ad_apb_sysmmu_tt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_TT_NS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_tt_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_TT_S, NULL, cmucal_vclk_ip_ad_apb_sysmmu_tt_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_UFD, NULL, cmucal_vclk_ip_slh_axi_mi_d_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA, NULL, cmucal_vclk_ip_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA, NULL, cmucal_vclk_ip_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DIT, NULL, cmucal_vclk_ip_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_TT, NULL, cmucal_vclk_ip_xiu_d_tt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_TT, NULL, cmucal_vclk_ip_sysmmu_tt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_DIT, NULL, cmucal_vclk_ip_sysmmu_s2_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_NOCL1B, NULL, cmucal_vclk_ip_vgen_lite_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_SPDMA, NULL, cmucal_vclk_ip_vgen_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_PDMA, NULL, cmucal_vclk_ip_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_TT, NULL, cmucal_vclk_ip_ad_axi_tt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_ID_DIT, NULL, cmucal_vclk_ip_lh_acel_mi_id_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_SI_ID_DIT, NULL, cmucal_vclk_ip_lh_acel_si_id_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_LITE_NOCL1B, NULL, cmucal_vclk_ip_ad_apb_vgen_lite_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_PDMA, NULL, cmucal_vclk_ip_ad_apb_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_SPDMA, NULL, cmucal_vclk_ip_ad_apb_vgen_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_IG_PPMU_TREXP, NULL, cmucal_vclk_ip_slh_astl_si_ig_ppmu_trexp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_IG_PPMU_XIU_D_TT, NULL, cmucal_vclk_ip_slh_astl_si_ig_ppmu_xiu_d_tt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_IG_PPMU_DIT, NULL, cmucal_vclk_ip_slh_astl_si_ig_ppmu_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_IG_PPMU_TREXP, NULL, cmucal_vclk_ip_slh_astl_mi_ig_ppmu_trexp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_IG_PPMU_XIU_D_TT, NULL, cmucal_vclk_ip_slh_astl_mi_ig_ppmu_xiu_d_tt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_IG_PPMU_DIT, NULL, cmucal_vclk_ip_slh_astl_mi_ig_ppmu_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_MI_IG_DEBUG_MUX_NOCL1B, NULL, cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_SI_IG_DEBUG_MUX_NOCL1B, NULL, cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl1b, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NOCL2A_CMU_NOCL2A, NULL, cmucal_vclk_ip_nocl2a_cmu_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D2_MCSC, NULL, cmucal_vclk_ip_lh_axi_mi_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NOCL2A, NULL, cmucal_vclk_ip_sysreg_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_NOCL2A, NULL, cmucal_vclk_ip_trex_d_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CSTAT, NULL, cmucal_vclk_ip_slh_axi_si_p_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_CSTAT, NULL, cmucal_vclk_ip_lh_axi_mi_d_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_MCFP, NULL, cmucal_vclk_ip_lh_axi_mi_d1_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_CSIS, NULL, cmucal_vclk_ip_slh_axi_si_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MCFP, NULL, cmucal_vclk_ip_slh_axi_si_p_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D4_MCFP, NULL, cmucal_vclk_ip_slh_axi_mi_d4_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_RGBP, NULL, cmucal_vclk_ip_lh_axi_mi_d0_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_BYRP, NULL, cmucal_vclk_ip_slh_axi_si_p_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ACEL_MI_D0_MCSC, NULL, cmucal_vclk_ip_lh_acel_mi_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_NOCL2A, NULL, cmucal_vclk_ip_trex_p_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_MCFP, NULL, cmucal_vclk_ip_lh_axi_mi_d0_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_BYRP, NULL, cmucal_vclk_ip_lh_axi_mi_d_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_YUVP, NULL, cmucal_vclk_ip_slh_axi_si_p_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D3_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D3_MCFP, NULL, cmucal_vclk_ip_slh_axi_mi_d3_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_G_NOCL2A, NULL, cmucal_vclk_ip_lh_ast_si_g_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_MCSC, NULL, cmucal_vclk_ip_lh_axi_mi_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_MCSC, NULL, cmucal_vclk_ip_slh_axi_si_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D0_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D_YUVP, NULL, cmucal_vclk_ip_slh_axi_mi_d_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NOCL2A, NULL, cmucal_vclk_ip_d_tzpc_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_DRCP, NULL, cmucal_vclk_ip_slh_axi_si_p_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_P_RGBP, NULL, cmucal_vclk_ip_slh_axi_si_p_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D_DRCP, NULL, cmucal_vclk_ip_lh_axi_mi_d_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D2_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_D5_MCFP, NULL, cmucal_vclk_ip_slh_axi_mi_d5_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D2_MCFP, NULL, cmucal_vclk_ip_lh_axi_mi_d2_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D4_CSIS, NULL, cmucal_vclk_ip_lh_axi_mi_d4_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_D1_RGBP, NULL, cmucal_vclk_ip_lh_axi_mi_d1_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_BYRP, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_byrp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_CSIS, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_CSTAT, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_cstat, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_DRCP, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_drcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MCFP, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_MCSC, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_RGBP, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_G_PPMU_YUVP, NULL, cmucal_vclk_ip_slh_astl_mi_g_ppmu_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_NOCL2A, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_2X1_P0_NOCL2A, NULL, cmucal_vclk_ip_siu_2x1_p0_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_4X1_P0_NOCL2A, NULL, cmucal_vclk_ip_siu_4x1_p0_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_8X1_P1_NOCL2A, NULL, cmucal_vclk_ip_siu_8x1_p1_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_MI_IG_PPMU_NOCL2A, NULL, cmucal_vclk_ip_slh_astl_mi_ig_ppmu_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_IG_PPMU_NOCL2A, NULL, cmucal_vclk_ip_slh_astl_si_ig_ppmu_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_MI_IG_DEBUG_MUX_NOCL2A, NULL, cmucal_vclk_ip_slh_ast_mi_ig_debug_mux_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_SI_IG_DEBUG_MUX_NOCL2A, NULL, cmucal_vclk_ip_slh_ast_si_ig_debug_mux_nocl2a, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC0_CMU_PERIC0, NULL, cmucal_vclk_ip_peric0_cmu_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC0, NULL, cmucal_vclk_ip_sysreg_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC0, NULL, cmucal_vclk_ip_gpio_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_PERIC0, NULL, cmucal_vclk_ip_slh_axi_mi_p_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC0, NULL, cmucal_vclk_ip_d_tzpc_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI04_USI, NULL, cmucal_vclk_ip_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI04_I2C, NULL, cmucal_vclk_ip_usi04_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC1_CMU_PERIC1, NULL, cmucal_vclk_ip_peric1_cmu_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC1, NULL, cmucal_vclk_ip_gpio_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC1, NULL, cmucal_vclk_ip_d_tzpc_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_CSISPERIC1, NULL, cmucal_vclk_ip_slh_axi_mi_p_csisperic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_PERIC1, NULL, cmucal_vclk_ip_slh_axi_mi_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC1, NULL, cmucal_vclk_ip_sysreg_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_PERIC1, NULL, cmucal_vclk_ip_xiu_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C00, NULL, cmucal_vclk_ip_i3c00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C01, NULL, cmucal_vclk_ip_i3c01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C02, NULL, cmucal_vclk_ip_i3c02, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BT_UART, NULL, cmucal_vclk_ip_bt_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI07_USI, NULL, cmucal_vclk_ip_usi07_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI07_SPI_I2C, NULL, cmucal_vclk_ip_usi07_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI08_USI, NULL, cmucal_vclk_ip_usi08_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI08_SPI_I2C, NULL, cmucal_vclk_ip_usi08_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI09_I2C, NULL, cmucal_vclk_ip_usi09_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI10_I2C, NULL, cmucal_vclk_ip_usi10_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI09_USI, NULL, cmucal_vclk_ip_usi09_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI10_USI, NULL, cmucal_vclk_ip_usi10_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_MULTI_SLV_Q_CTRL_PERIC1, NULL, cmucal_vclk_ip_spi_multi_slv_q_ctrl_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC2_CMU_PERIC2, NULL, cmucal_vclk_ip_peric2_cmu_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC2, NULL, cmucal_vclk_ip_d_tzpc_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC2, NULL, cmucal_vclk_ip_gpio_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_PERIC2, NULL, cmucal_vclk_ip_slh_axi_mi_p_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC2, NULL, cmucal_vclk_ip_sysreg_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_SPI_I2C, NULL, cmucal_vclk_ip_usi00_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI01_SPI_I2C, NULL, cmucal_vclk_ip_usi01_spi_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI02_I2C, NULL, cmucal_vclk_ip_usi02_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI03_I2C, NULL, cmucal_vclk_ip_usi03_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI05_I2C, NULL, cmucal_vclk_ip_usi05_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI06_I2C, NULL, cmucal_vclk_ip_usi06_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI00_USI, NULL, cmucal_vclk_ip_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI01_USI, NULL, cmucal_vclk_ip_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI02_USI, NULL, cmucal_vclk_ip_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI03_USI, NULL, cmucal_vclk_ip_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI05_USI, NULL, cmucal_vclk_ip_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI06_USI, NULL, cmucal_vclk_ip_usi06_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C06, NULL, cmucal_vclk_ip_i3c06, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C07, NULL, cmucal_vclk_ip_i3c07, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C08, NULL, cmucal_vclk_ip_i3c08, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C09, NULL, cmucal_vclk_ip_i3c09, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C10, NULL, cmucal_vclk_ip_i3c10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C11, NULL, cmucal_vclk_ip_i3c11, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_MULTI_SLV_Q_CTRL_PERIC2, NULL, cmucal_vclk_ip_spi_multi_slv_q_ctrl_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI11_I2C, NULL, cmucal_vclk_ip_usi11_i2c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI11_USI, NULL, cmucal_vclk_ip_usi11_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C03, NULL, cmucal_vclk_ip_i3c03, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C04, NULL, cmucal_vclk_ip_i3c04, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C05, NULL, cmucal_vclk_ip_i3c05, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DBG_UART, NULL, cmucal_vclk_ip_dbg_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PWM, NULL, cmucal_vclk_ip_pwm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIS_CMU_PERIS, NULL, cmucal_vclk_ip_peris_cmu_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIS, NULL, cmucal_vclk_ip_d_tzpc_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC, NULL, cmucal_vclk_ip_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_LD_ICC_CPUGIC_CLUSTER0, NULL, cmucal_vclk_ip_lh_ast_mi_ld_icc_cpugic_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_PERIS, NULL, cmucal_vclk_ip_slh_axi_mi_p_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_PERISGIC, NULL, cmucal_vclk_ip_slh_axi_mi_p_perisgic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_IRI_GICCPU_CLUSTER0, NULL, cmucal_vclk_ip_lh_ast_si_ld_iri_giccpu_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIS, NULL, cmucal_vclk_ip_sysreg_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU_SUB, NULL, cmucal_vclk_ip_tmu_sub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU_TOP, NULL, cmucal_vclk_ip_tmu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT0, NULL, cmucal_vclk_ip_wdt0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT1, NULL, cmucal_vclk_ip_wdt1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_BIRA, NULL, cmucal_vclk_ip_otp_con_bira, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_PERIS, NULL, cmucal_vclk_ip_busif_ddd_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_PERIS, NULL, cmucal_vclk_ip_ddd_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RGBP_CMU_RGBP, NULL, cmucal_vclk_ip_rgbp_cmu_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_RGBP, NULL, cmucal_vclk_ip_ad_apb_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_RGBP, NULL, cmucal_vclk_ip_d_tzpc_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_RGBP, NULL, cmucal_vclk_ip_slh_axi_mi_p_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D0_RGBP, NULL, cmucal_vclk_ip_lh_axi_si_d0_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_RGBP, NULL, cmucal_vclk_ip_ppmu_d0_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_RGBP, NULL, cmucal_vclk_ip_sysmmu_d0_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_RGBP, NULL, cmucal_vclk_ip_sysreg_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_RGBP, NULL, cmucal_vclk_ip_xiu_d0_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RGBP, NULL, cmucal_vclk_ip_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF_BYRPRGBP, NULL, cmucal_vclk_ip_lh_ast_mi_otf_byrprgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF_RGBPMCFP, NULL, cmucal_vclk_ip_lh_ast_si_otf_rgbpmcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_RGBP, NULL, cmucal_vclk_ip_vgen_lite_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_RGBP, NULL, cmucal_vclk_ip_ppmu_d1_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_D1_RGBP, NULL, cmucal_vclk_ip_lh_axi_si_d1_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_RGBP, NULL, cmucal_vclk_ip_xiu_d1_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_RGBP, NULL, cmucal_vclk_ip_sysmmu_d1_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_RGBP, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIU_G_PPMU_RGBP, NULL, cmucal_vclk_ip_siu_g_ppmu_rgbp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2D_CMU_S2D, NULL, cmucal_vclk_ip_s2d_cmu_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BIS_S2D, NULL, cmucal_vclk_ip_bis_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_slh_axi_mi_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SDMA_CMU_SDMA, NULL, cmucal_vclk_ip_sdma_cmu_sdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA5, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA4, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA7, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data7, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA6, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA1, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA0, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA3, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_DATA2, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_data2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_MMU1, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_MMU2, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_SDMA, NULL, cmucal_vclk_ip_slh_axi_mi_p_sdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_SDMA, NULL, cmucal_vclk_ip_sysreg_sdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_MMU0, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_SI_LD_SDMADNC_MMU3, NULL, cmucal_vclk_ip_lh_axi_si_ld_sdmadnc_mmu3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_SDMA, NULL, cmucal_vclk_ip_d_tzpc_sdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_STRM_SDMADSP0, NULL, cmucal_vclk_ip_lh_ast_si_ld_strm_sdmadsp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_LD_STRM_SDMADSP1, NULL, cmucal_vclk_ip_lh_ast_si_ld_strm_sdmadsp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_SDMA, NULL, cmucal_vclk_ip_ip_sdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_LP_DNCSDMA, NULL, cmucal_vclk_ip_lh_axi_mi_lp_dncsdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSP_CMU_SSP, NULL, cmucal_vclk_ip_ssp_cmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_SSS, NULL, cmucal_vclk_ip_baaw_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_P_SSP, NULL, cmucal_vclk_ip_bps_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_SSP, NULL, cmucal_vclk_ip_d_tzpc_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AXI_MI_L_STRONG, NULL, cmucal_vclk_ip_lh_axi_mi_l_strong, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_SSP, NULL, cmucal_vclk_ip_slh_axi_mi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ACEL_SI_D_SSP, NULL, cmucal_vclk_ip_slh_acel_si_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_SSP, NULL, cmucal_vclk_ip_ppmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_RTIC, NULL, cmucal_vclk_ip_qe_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRONG, NULL, cmucal_vclk_ip_qe_strong, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_SSS, NULL, cmucal_vclk_ip_qe_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTIC, NULL, cmucal_vclk_ip_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_SSP, NULL, cmucal_vclk_ip_sweeper_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_SSP, NULL, cmucal_vclk_ip_sysreg_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_SSP, NULL, cmucal_vclk_ip_vgen_lite_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_SSP, NULL, cmucal_vclk_ip_xiu_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_SSP, NULL, cmucal_vclk_ip_ad_apb_sysmmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_SSP, NULL, cmucal_vclk_ip_sysmmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_US_32TO128_SSSCORE, NULL, cmucal_vclk_ip_us_32to128_ssscore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_G_SSS, NULL, cmucal_vclk_ip_adm_dap_g_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_SSP, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_STRONG_CMU_STRONG, NULL, cmucal_vclk_ip_strong_cmu_strong, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFD_CMU_UFD, NULL, cmucal_vclk_ip_ufd_cmu_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_UFD, NULL, cmucal_vclk_ip_d_tzpc_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LP_CMGPUFD, NULL, cmucal_vclk_ip_slh_axi_mi_lp_cmgpufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_UFD, NULL, cmucal_vclk_ip_slh_axi_mi_p_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_UFD, NULL, cmucal_vclk_ip_pdma_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_UFD, NULL, cmucal_vclk_ip_sysreg_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFD, NULL, cmucal_vclk_ip_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_UFD, NULL, cmucal_vclk_ip_xiu_dp_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPI_UFD, NULL, cmucal_vclk_ip_spi_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D_UFD, NULL, cmucal_vclk_ip_vgen_lite_d_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_UFD, NULL, cmucal_vclk_ip_slh_axi_si_d_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_UFD, NULL, cmucal_vclk_ip_xiu_d_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LD_UFDDNC, NULL, cmucal_vclk_ip_slh_axi_si_ld_ufddnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SRAM_MIU_UFD, NULL, cmucal_vclk_ip_sram_miu_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_UFD, NULL, cmucal_vclk_ip_i3c_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_UFDDNC, NULL, cmucal_vclk_ip_baaw_d_ufddnc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_UFD_SECURE, NULL, cmucal_vclk_ip_sysreg_ufd_secure, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ID_COMP_UFD, NULL, cmucal_vclk_ip_id_comp_ufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_LP_UFDCSIS, NULL, cmucal_vclk_ip_slh_axi_si_lp_ufdcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AST_MI_OTF_CSISUFD, NULL, cmucal_vclk_ip_slh_ast_mi_otf_csisufd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB2AXI_AHB0, NULL, cmucal_vclk_ip_ahb2axi_ahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB2AXI_AHB4, NULL, cmucal_vclk_ip_ahb2axi_ahb4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAXI0, NULL, cmucal_vclk_ip_asyncaxi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAXI2, NULL, cmucal_vclk_ip_asyncaxi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAXI4, NULL, cmucal_vclk_ip_asyncaxi4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_C_VTS, NULL, cmucal_vclk_ip_baaw_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_LP_VTS, NULL, cmucal_vclk_ip_bps_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_P_VTS, NULL, cmucal_vclk_ip_bps_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMAILBOX_TEST, NULL, cmucal_vclk_ip_dmailbox_test, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB0, NULL, cmucal_vclk_ip_dmic_ahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB2, NULL, cmucal_vclk_ip_dmic_ahb2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB4, NULL, cmucal_vclk_ip_dmic_ahb4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF0, NULL, cmucal_vclk_ip_dmic_if0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF1, NULL, cmucal_vclk_ip_dmic_if1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF2, NULL, cmucal_vclk_ip_dmic_if2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VTS, NULL, cmucal_vclk_ip_d_tzpc_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_VTS, NULL, cmucal_vclk_ip_gpio_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC0, NULL, cmucal_vclk_ip_hwacg_sys_dmic0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC2, NULL, cmucal_vclk_ip_hwacg_sys_dmic2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC4, NULL, cmucal_vclk_ip_hwacg_sys_dmic4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM_CODE, NULL, cmucal_vclk_ip_intmem_code, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM_DATA0, NULL, cmucal_vclk_ip_intmem_data0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM_DATA1, NULL, cmucal_vclk_ip_intmem_data1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM_PCM, NULL, cmucal_vclk_ip_intmem_pcm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_AUDVTS, NULL, cmucal_vclk_ip_slh_axi_mi_ld_audvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LP_VTS, NULL, cmucal_vclk_ip_slh_axi_mi_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_VTS, NULL, cmucal_vclk_ip_slh_axi_mi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_C_VTS, NULL, cmucal_vclk_ip_slh_axi_si_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_ABOX_VTS, NULL, cmucal_vclk_ip_mailbox_abox_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_VTS, NULL, cmucal_vclk_ip_mailbox_ap_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_VTS, NULL, cmucal_vclk_ip_pdma_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SERIAL_LIF_VT, NULL, cmucal_vclk_ip_serial_lif_vt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_VTS_GLUE, NULL, cmucal_vclk_ip_ss_vts_glue, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_C_VTS, NULL, cmucal_vclk_ip_sweeper_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VTS, NULL, cmucal_vclk_ip_sysreg_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE, NULL, cmucal_vclk_ip_vgen_lite, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_VTS, NULL, cmucal_vclk_ip_wdt_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB2AXI_AHB2, NULL, cmucal_vclk_ip_ahb2axi_ahb2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP1_VTS, NULL, cmucal_vclk_ip_xiu_dp1_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VTS_CMU_VTS, NULL, cmucal_vclk_ip_vts_cmu_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_LD_DNCVTS, NULL, cmucal_vclk_ip_slh_axi_mi_ld_dncvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER, NULL, cmucal_vclk_ip_timer, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER1, NULL, cmucal_vclk_ip_timer1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER2, NULL, cmucal_vclk_ip_timer2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_YAMIN_MCU_VTS, NULL, cmucal_vclk_ip_yamin_mcu_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_DNC_VTS, NULL, cmucal_vclk_ip_mailbox_dnc_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAXI_SERIAL_LIF_VT, NULL, cmucal_vclk_ip_asyncaxi_serial_lif_vt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCINTERRUPT_VTS, NULL, cmucal_vclk_ip_asyncinterrupt_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_YUVP_CMU_YUVP, NULL, cmucal_vclk_ip_yuvp_cmu_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_YUVP, NULL, cmucal_vclk_ip_ad_apb_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_MI_P_YUVP, NULL, cmucal_vclk_ip_slh_axi_mi_p_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_YUVP, NULL, cmucal_vclk_ip_sysreg_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_AXI_SI_D_YUVP, NULL, cmucal_vclk_ip_slh_axi_si_d_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_YUVP, NULL, cmucal_vclk_ip_ppmu_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_YUVP, NULL, cmucal_vclk_ip_sysmmu_d_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_YUVP, NULL, cmucal_vclk_ip_xiu_d_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_YUVP, NULL, cmucal_vclk_ip_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_MI_OTF0_MCFPYUVP, NULL, cmucal_vclk_ip_lh_ast_mi_otf0_mcfpyuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF_YUVPDRCP, NULL, cmucal_vclk_ip_lh_ast_si_otf_yuvpdrcp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_YUVP, NULL, cmucal_vclk_ip_d_tzpc_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_AST_SI_OTF_YUVPMCFP, NULL, cmucal_vclk_ip_lh_ast_si_otf_yuvpmcfp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_YUVP, NULL, cmucal_vclk_ip_hpm_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_YUVP, NULL, cmucal_vclk_ip_busif_hpm_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_YUVP, NULL, cmucal_vclk_ip_busif_ddd_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_YUVP, NULL, cmucal_vclk_ip_busif_add_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_YUVP, NULL, cmucal_vclk_ip_add_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_YUVP, NULL, cmucal_vclk_ip_ddd_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_YUVP, NULL, cmucal_vclk_ip_vgen_lite_yuvp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SLH_ASTL_SI_G_PPMU_YUVP, NULL, cmucal_vclk_ip_slh_astl_si_g_ppmu_yuvp, NULL, NULL),
};

