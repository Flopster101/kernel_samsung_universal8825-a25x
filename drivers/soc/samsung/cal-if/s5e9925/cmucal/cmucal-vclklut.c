#include "../../cmucal.h"
#include "cmucal-vclklut.h"


/* DVFS VCLK -> LUT Parameter List */
unsigned int vddi_uud_lut_params[] = {
	0, 0, 0, 0, 1, 0, 0, 0, 
};
unsigned int vddi_ud_lut_params[] = {
	1, 1, 1, 1, 2, 2, 2, 1, 
};
unsigned int vddi_sud_lut_params[] = {
	1, 1, 1, 1, 2, 2, 2, 1, 
};
unsigned int vddi_nm_lut_params[] = {
	1, 1, 1, 1, 2, 2, 2, 1, 
};
unsigned int vdd_mif_od_lut_params[] = {
	4266000, 4266000, 
};
unsigned int vdd_mif_ud_lut_params[] = {
	3080000, 3080000, 
};
unsigned int vdd_mif_sud_lut_params[] = {
	1690000, 1690000, 
};
unsigned int vdd_mif_uud_lut_params[] = {
	842000, 842000, 
};
unsigned int vdd_g3d_nm_lut_params[] = {
	1300000, 1300000, 
};
unsigned int vdd_g3d_ud_lut_params[] = {
	1025000, 1025000, 
};
unsigned int vdd_g3d_sud_lut_params[] = {
	750000, 750000, 
};
unsigned int vdd_g3d_uud_lut_params[] = {
	450000, 450000, 
};
unsigned int vdd_cam_uud_lut_params[] = {
	0, 0, 0, 1, 0, 
};
unsigned int vdd_cam_ud_lut_params[] = {
	1, 1, 1, 2, 1, 
};
unsigned int vdd_cam_sud_lut_params[] = {
	1, 1, 1, 2, 1, 
};
unsigned int vdd_cam_nm_lut_params[] = {
	1, 1, 1, 2, 1, 
};
unsigned int vdd_cpucl2_sod_lut_params[] = {
	2950000, 0, 
};
unsigned int vdd_cpucl2_od_lut_params[] = {
	2500000, 0, 
};
unsigned int vdd_cpucl2_nm_lut_params[] = {
	2000000, 0, 
};
unsigned int vdd_cpucl2_ud_lut_params[] = {
	1400000, 0, 
};
unsigned int vdd_cpucl2_sud_lut_params[] = {
	700000, 0, 
};
unsigned int vdd_cpucl2_uud_lut_params[] = {
	300000, 2, 
};
unsigned int vdd_cpucl0_sod_lut_params[] = {
	2100000, 2089000, 1, 1, 1, 1, 0, 3, 0, 
};
unsigned int vdd_cpucl0_od_lut_params[] = {
	1900000, 1774000, 1, 1, 1, 1, 0, 3, 0, 
};
unsigned int vdd_cpucl0_nm_lut_params[] = {
	1700000, 1467000, 0, 0, 0, 3, 0, 3, 0, 
};
unsigned int vdd_cpucl0_ud_lut_params[] = {
	1200000, 982000, 0, 0, 0, 3, 0, 3, 0, 
};
unsigned int vdd_cpucl0_sud_lut_params[] = {
	600000, 540000, 0, 0, 0, 2, 0, 3, 0, 
};
unsigned int vdd_cpucl0_uud_lut_params[] = {
	200000, 200000, 0, 0, 0, 1, 2, 1, 2, 
};
unsigned int vdd_cpucl1_sod_lut_params[] = {
	2700000, 0, 
};
unsigned int vdd_cpucl1_od_lut_params[] = {
	2250000, 0, 
};
unsigned int vdd_cpucl1_nm_lut_params[] = {
	1850000, 0, 
};
unsigned int vdd_cpucl1_ud_lut_params[] = {
	1300000, 0, 
};
unsigned int vdd_cpucl1_sud_lut_params[] = {
	650000, 0, 
};
unsigned int vdd_cpucl1_uud_lut_params[] = {
	300000, 2, 
};
unsigned int vdd_alive_uud_lut_params[] = {
	0, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};
unsigned int vdd_alive_ud_lut_params[] = {
	0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 1, 1, 1, 1, 
};
unsigned int vdd_alive_sud_lut_params[] = {
	1, 3, 3, 3, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 
};
unsigned int vdd_alive_nm_lut_params[] = {
	0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 
};
unsigned int vdd_npu_sod_lut_params[] = {
	1550000, 7, 7, 0, 2, 5, 3, 3, 1, 1, 1, 1, 1, 1, 3, 3, 
};
unsigned int vdd_npu_nm_lut_params[] = {
	1200000, 5, 7, 0, 2, 5, 3, 3, 1, 1, 1, 1, 1, 1, 3, 3, 
};
unsigned int vdd_npu_ud_lut_params[] = {
	800000, 9, 7, 1, 1, 2, 3, 3, 1, 1, 1, 1, 1, 1, 3, 3, 
};
unsigned int vdd_npu_sud_lut_params[] = {
	400000, 4, 3, 1, 0, 1, 2, 2, 1, 1, 1, 1, 1, 1, 3, 3, 
};
unsigned int vdd_npu_uud_lut_params[] = {
	400000, 7, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
};
unsigned int vdd_int_cmu_nm_lut_params[] = {
	4, 4, 2, 1, 1, 1, 1, 2, 3, 2, 0, 3, 3, 1, 1, 3, 3, 1, 2, 4, 1, 2, 1, 2, 2, 1, 1, 0, 1, 1, 1, 1, 2, 2, 1, 1, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 808000, 0, 0, 1, 2028000, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, 1, 3, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 
};
unsigned int vdd_int_cmu_od_lut_params[] = {
	0, 0, 1, 0, 0, 0, 0, 1, 2, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 2, 1, 1, 1, 1, 2, 2, 1, 1, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 808000, 0, 0, 1, 2028000, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, 1, 3, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 
};
unsigned int vdd_int_cmu_sod_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 808000, 0, 0, 1, 2028000, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 1, 0, 0, 0, 1, 3, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 
};
unsigned int vdd_int_cmu_ud_lut_params[] = {
	4, 4, 4, 3, 3, 1, 1, 5, 3, 0, 1, 5, 5, 3, 3, 5, 5, 3, 2, 4, 1, 2, 3, 5, 5, 3, 3, 0, 1, 1, 0, 1, 2, 2, 3, 1, 3, 2, 2, 2, 2, 0, 3, 0, 0, 4, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 3, 0, 0, 808000, 0, 1, 1, 2028000, 3, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 2, 1, 0, 1, 3, 3, 2, 0, 0, 3, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 1, 3, 0, 1, 0, 0, 0, 1, 3, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 
};
unsigned int vdd_int_cmu_sud_lut_params[] = {
	4, 4, 1, 2, 2, 1, 1, 5, 0, 4, 1, 3, 3, 2, 2, 3, 3, 2, 2, 4, 1, 2, 2, 5, 5, 2, 2, 0, 1, 1, 1, 1, 2, 2, 2, 1, 3, 2, 2, 2, 2, 1, 2, 1, 1, 2, 5, 1, 1, 1, 2, 1, 1, 1, 1, 1, 5, 4, 1, 1, 1, 1, 1, 1, 2, 1, 1, 404000, 1, 1, 3, 1690000, 3, 3, 4, 1, 5, 1, 1, 4, 1, 1, 1, 1, 2, 1, 0, 1, 2, 2, 1, 1, 1, 2, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 3, 0, 0, 1, 0, 0, 0, 1, 3, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 
};
unsigned int vdd_int_cmu_uud_lut_params[] = {
	4, 4, 1, 2, 3, 1, 1, 4, 0, 1, 5, 3, 3, 4, 2, 3, 3, 2, 2, 4, 1, 2, 3, 1, 1, 2, 3, 0, 1, 1, 1, 3, 2, 2, 3, 1, 3, 2, 2, 2, 2, 5, 4, 2, 11, 1, 2, 5, 7, 5, 2, 7, 3, 9, 1, 5, 2, 4, 5, 5, 5, 7, 5, 5, 4, 2, 7, 101000, 5, 3, 3, 842000, 5, 7, 2, 1, 2, 5, 11, 2, 11, 1, 2, 5, 2, 1, 0, 3, 2, 2, 0, 5, 7, 2, 7, 5, 0, 5, 7, 3, 11, 11, 11, 9, 7, 7, 5, 5, 5, 5, 5, 2, 1, 0, 0, 1, 3, 3, 3, 1, 1, 3, 3, 4, 3, 1, 1, 1, 1, 1, 5, 3, 3, 3, 3, 3, 3, 3, 1, 
};
unsigned int vdd_chubvts_ud_lut_params[] = {
	0, 0, 0, 
};
unsigned int vdd_chubvts_sud_lut_params[] = {
	1, 1, 1, 
};
unsigned int vdd_chubvts_nm_lut_params[] = {
	0, 0, 0, 
};

/* SPECIAL VCLK -> LUT Parameter List */
unsigned int mux_chub_timer_uud_lut_params[] = {
	0, 
};
unsigned int mux_cpucl0_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clkcmu_cmu_boost_cpu_uud_lut_params[] = {
	2, 
};
unsigned int mux_cpucl1_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_cpucl2_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_dsu_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clk_dsu_powerip_sod_lut_params[] = {
	0, 
};
unsigned int mux_clk_dsu_powerip_od_lut_params[] = {
	0, 
};
unsigned int mux_clk_dsu_powerip_nm_lut_params[] = {
	0, 
};
unsigned int mux_clk_dsu_powerip_ud_lut_params[] = {
	0, 
};
unsigned int mux_clk_dsu_powerip_sud_lut_params[] = {
	0, 
};
unsigned int mux_clk_dsu_powerip_uud_lut_params[] = {
	0, 
};
unsigned int mux_clk_hsi0_usb32drd_uud_lut_params[] = {
	1, 1, 
};
unsigned int clkcmu_hsi0_usb32drd_uud_lut_params[] = {
	0, 0, 
};
unsigned int mux_mif_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clkcmu_cmu_boost_mif_uud_lut_params[] = {
	2, 
};
unsigned int mux_nocl0_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clkcmu_cmu_boost_uud_lut_params[] = {
	2, 
};
unsigned int mux_nocl1a_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_nocl1b_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_nocl1c_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clkcmu_cmu_boost_cam_uud_lut_params[] = {
	2, 
};
unsigned int clkcmu_dpub_dsim_uud_lut_params[] = {
	6, 0, 
};
unsigned int clkcmu_hsi0_dposc_uud_lut_params[] = {
	0, 0, 
};
unsigned int div_clk_alive_spmi_uud_lut_params[] = {
	15, 1, 
};
unsigned int div_clk_alive_dbgcore_uart_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_alive_pmu_sub_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_aud_dsif_uud_lut_params[] = {
	2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 0, 2, 0, 2, 0, 0, 0, 0, 1, 
};
unsigned int clkcmu_aud_cpu_sod_lut_params[] = {
	0, 
};
unsigned int clkcmu_aud_cpu_ud_lut_params[] = {
	0, 
};
unsigned int clkcmu_aud_cpu_sud_lut_params[] = {
	0, 
};
unsigned int clkcmu_aud_cpu_uud_lut_params[] = {
	0, 
};
unsigned int clkcmu_aud_audif0_uud_lut_params[] = {
	0, 8, 3, 8, 
};
unsigned int clkcmu_aud_audif1_uud_lut_params[] = {
	0, 10, 2, 10, 
};
unsigned int div_clk_aud_serial_lif_uud_lut_params[] = {
	0, 3, 1, 
};
unsigned int clk_aud_mclk_uud_lut_params[] = {
	0, 
};
unsigned int clk_brp_add_ch_clk_uud_lut_params[] = {
	8, 
};
unsigned int div_clk_chub_usi0_uud_lut_params[] = {
	0, 1, 
};
unsigned int clkalive_chub_peri_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_chub_usi1_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_chub_usi3_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_chub_usi2_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_chub_spi_ms_ctrl_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_chub_spi_i2c0_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_chub_spi_i2c1_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_chubvts_dmailbox_cclk_ud_lut_params[] = {
	0, 1, 
};
unsigned int clkalive_chubvts_noc_ud_lut_params[] = {
	0, 
};
unsigned int clkalive_chubvts_noc_uud_lut_params[] = {
	0, 
};
unsigned int clkalive_chubvts_noc_sud_lut_params[] = {
	0, 
};
unsigned int div_clk_cmgp_usi4_nm_lut_params[] = {
	0, 1, 
};
unsigned int clkalive_cmgp_peri_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi1_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi0_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi2_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi3_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi5_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi6_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_spi_ms_ctrl_nm_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_cmgp_spi_i2c0_nm_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_cmgp_spi_i2c1_nm_lut_params[] = {
	1, 1, 
};
unsigned int div_clkcmu_cis_clk0_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clkcmu_cis_clk1_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clkcmu_cis_clk2_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clkcmu_cis_clk3_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clkcmu_cis_clk4_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clkcmu_cis_clk5_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clkcmu_cis_clk6_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clkcmu_cis_clk7_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clk_cpucl1_core_1_sod_lut_params[] = {
	0, 0, 
};
unsigned int div_clk_cpucl1_core_1_od_lut_params[] = {
	0, 0, 
};
unsigned int div_clk_cpucl1_core_1_nm_lut_params[] = {
	0, 0, 
};
unsigned int div_clk_cpucl1_core_1_ud_lut_params[] = {
	0, 0, 
};
unsigned int div_clk_cpucl1_core_1_sud_lut_params[] = {
	0, 0, 
};
unsigned int div_clk_cpucl1_core_1_uud_lut_params[] = {
	0, 0, 
};
unsigned int div_clk_csis_dcphy_uud_lut_params[] = {
	0, 0, 
};
unsigned int clk_dnc_add_ch_clk_uud_lut_params[] = {
	8, 
};
unsigned int clk_dsu_str_dem_clk_uud_lut_params[] = {
	9, 
};
unsigned int clk_g3d_add_ch_clk_uud_lut_params[] = {
	2, 
};
unsigned int div_clk_peric0_usi04_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric1_uart_bt_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric1_usi07_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric1_usi08_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric1_usi09_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric1_usi10_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric1_spi_ms_ctrl_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric1_usi07_spi_i2c_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric1_usi08_spi_i2c_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric2_usi00_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric2_usi01_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric2_usi02_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric2_usi03_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric2_usi05_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric2_usi06_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric2_spi_ms_ctrl_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric2_usi11_uud_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_peric2_uart_dbg_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric2_usi00_spi_i2c_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric2_usi01_spi_i2c_uud_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_vts_serial_lif_uud_lut_params[] = {
	0, 0, 
};
unsigned int clkcmu_g3d_switch_sod_lut_params[] = {
	0, 
};
unsigned int clkcmu_g3d_switch_ud_lut_params[] = {
	0, 
};
unsigned int clkcmu_g3d_switch_sud_lut_params[] = {
	0, 
};
unsigned int clkcmu_g3d_switch_uud_lut_params[] = {
	0, 
};
unsigned int mux_clkalive_gnss_noc_uud_lut_params[] = {
	0, 
};
unsigned int clkcmu_dnc_noc_sod_lut_params[] = {
	0, 
};
unsigned int clkcmu_dnc_noc_nm_lut_params[] = {
	0, 
};
unsigned int clkcmu_dnc_noc_ud_lut_params[] = {
	0, 
};
unsigned int clkcmu_dnc_noc_sud_lut_params[] = {
	0, 
};
unsigned int clkcmu_dnc_noc_uud_lut_params[] = {
	0, 
};
unsigned int mux_clkcmu_mif_switch_ud_lut_params[] = {
	7, 
};
unsigned int mux_clkcmu_mif_switch_sud_lut_params[] = {
	7, 
};
unsigned int mux_clkcmu_mif_switch_uud_lut_params[] = {
	7, 
};

/* COMMON VCLK -> LUT Parameter List */
unsigned int blk_cmu_sod_lut_params[] = {
	1066000, 936000, 800000, 753000, 670000, 0, 0, 1, 1, 0, 0, 0, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 1, 0, 5, 2, 0, 1, 0, 0, 
};
unsigned int blk_cmu_ud_lut_params[] = {
	1066000, 936000, 800000, 753000, 670000, 0, 0, 1, 1, 0, 0, 0, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 1, 0, 5, 2, 0, 1, 0, 0, 
};
unsigned int blk_cmu_uud_lut_params[] = {
	1066000, 936000, 800000, 753000, 670000, 0, 0, 1, 1, 0, 0, 0, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 1, 0, 5, 2, 0, 1, 0, 0, 
};
unsigned int blk_s2d_nm_lut_params[] = {
	400000, 1, 
};
unsigned int blk_alive_uud_lut_params[] = {
	1, 1, 1, 
};
unsigned int blk_aud_uud_lut_params[] = {
	1, 0, 0, 0, 
};
unsigned int blk_chub_uud_lut_params[] = {
	1, 0, 1, 
};
unsigned int blk_chubvts_ud_lut_params[] = {
	0, 
};
unsigned int blk_cmgp_nm_lut_params[] = {
	1, 1, 
};
unsigned int blk_cpucl0_sod_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl0_od_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl0_nm_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl0_ud_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl0_sud_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl0_uud_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl1_sod_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 1, 2, 2, 
};
unsigned int blk_cpucl1_od_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 1, 2, 2, 
};
unsigned int blk_cpucl1_nm_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 1, 2, 2, 
};
unsigned int blk_cpucl1_ud_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 1, 2, 2, 
};
unsigned int blk_cpucl1_sud_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 1, 2, 2, 
};
unsigned int blk_cpucl1_uud_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 1, 2, 2, 
};
unsigned int blk_cpucl2_sod_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl2_od_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl2_nm_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl2_ud_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl2_sud_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_cpucl2_uud_lut_params[] = {
	0, 0, 0, 0, 9, 0, 
};
unsigned int blk_csis_uud_lut_params[] = {
	0, 
};
unsigned int blk_dnc_uud_lut_params[] = {
	0, 
};
unsigned int blk_dsp_uud_lut_params[] = {
	0, 
};
unsigned int blk_dsu_sod_lut_params[] = {
	0, 0, 0, 1, 0, 
};
unsigned int blk_dsu_od_lut_params[] = {
	0, 0, 0, 1, 0, 
};
unsigned int blk_dsu_nm_lut_params[] = {
	0, 0, 0, 1, 0, 
};
unsigned int blk_dsu_ud_lut_params[] = {
	0, 0, 0, 1, 0, 
};
unsigned int blk_dsu_sud_lut_params[] = {
	0, 0, 0, 1, 0, 
};
unsigned int blk_dsu_uud_lut_params[] = {
	0, 0, 0, 1, 0, 
};
unsigned int blk_g3dcore_nm_lut_params[] = {
	0, 0, 0, 0, 0, 0, 
};
unsigned int blk_g3dcore_ud_lut_params[] = {
	0, 0, 0, 0, 0, 0, 
};
unsigned int blk_g3dcore_sud_lut_params[] = {
	0, 0, 0, 0, 0, 0, 
};
unsigned int blk_g3dcore_uud_lut_params[] = {
	0, 0, 0, 0, 0, 0, 
};
unsigned int blk_gnpu_uud_lut_params[] = {
	0, 
};
unsigned int blk_hsi0_uud_lut_params[] = {
	0, 
};
unsigned int blk_peric0_uud_lut_params[] = {
	1, 1, 
};
unsigned int blk_peric1_uud_lut_params[] = {
	1, 1, 
};
unsigned int blk_peric2_uud_lut_params[] = {
	1, 1, 
};
unsigned int blk_sdma_uud_lut_params[] = {
	0, 
};
unsigned int blk_ufd_uud_lut_params[] = {
	0, 
};
unsigned int blk_vts_uud_lut_params[] = {
	0, 0, 0, 7, 1, 0, 0, 15, 
};
unsigned int blk_cpucl0_glb_uud_lut_params[] = {
	1, 
};
unsigned int blk_mfc0_uud_lut_params[] = {
	1, 
};
unsigned int blk_mfc1_uud_lut_params[] = {
	1, 
};
