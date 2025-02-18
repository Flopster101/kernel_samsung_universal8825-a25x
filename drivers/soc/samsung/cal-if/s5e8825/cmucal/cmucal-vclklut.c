#include "../../cmucal.h"
#include "cmucal-vclklut.h"


/* DVFS VCLK -> LUT Parameter List */
unsigned int vdd_mif_nm_lut_params[] = {
	4266000,
};
unsigned int vdd_mif_ud_lut_params[] = {
	3078000,
};
unsigned int vdd_mif_sud_lut_params[] = {
	1410000,
};
unsigned int vdd_mif_uud_lut_params[] = {
	710000,
};
unsigned int vdd_cpucl0_sod_lut_params[] = {
	2000000, 1800000, 1,
};
unsigned int vdd_cpucl0_od_lut_params[] = {
	1700000, 1500000, 1,
};
unsigned int vdd_cpucl0_nm_lut_params[] = {
	1500000, 1200000, 1,
};
unsigned int vdd_cpucl0_ud_lut_params[] = {
	1000000, 800000, 0,
};
unsigned int vdd_cpucl0_sud_lut_params[] = {
	450000, 400000, 0,
};
unsigned int vdd_cpucl0_uud_lut_params[] = {
	200000, 150000, 0,
};
unsigned int vdd_alive_uud_lut_params[] = {
	0, 0, 0,
};
unsigned int vdd_alive_nm_lut_params[] = {
	1, 1, 1,
};
unsigned int vddi_od_lut_params[] = {
	0, 0, 0, 0, 0, 1, 0, 1599000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1100000, 0, 0, 0, 808000, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2, 0, 0, 1, 1, 0, 0, 1,
};
unsigned int vddi_sud_lut_params[] = {
	1, 3, 1, 3, 1, 0, 3, 1599000, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 0, 1, 1, 1, 1, 400000, 1, 1, 1, 808000, 1, 1, 1, 1, 1, 2, 1, 1, 7, 0, 3, 2, 1, 2, 3, 1, 1, 2, 1, 2, 0, 0, 1, 1, 0, 0, 1,
};
unsigned int vddi_ud_lut_params[] = {
	0, 1, 0, 3, 0, 0, 1, 1599000, 0, 0, 2, 0, 0, 2, 0, 0, 1, 1, 2, 0, 1, 1, 2, 0, 2, 0, 600000, 0, 0, 0, 808000, 2, 0, 0, 2, 0, 2, 0, 2, 3, 0, 1, 1, 0, 2, 1, 2, 0, 0, 1, 0, 2, 0, 1, 1, 0, 0, 1,
};
unsigned int vddi_uud_lut_params[] = {
	3, 15, 7, 3, 7, 0, 15, 1599000, 3, 2, 1, 7, 7, 1, 7, 4, 3, 3, 2, 1, 1, 0, 1, 3, 1, 7, 200000, 0, 7, 7, 806000, 1, 7, 7, 1, 7, 2, 1, 1, 15, 0, 15, 2, 3, 2, 7, 1, 7, 0, 7, 2, 0, 2, 7, 7, 3, 2, 7,
};
unsigned int vdd_cpucl1_sod_lut_params[] = {
	2600000, 1,
};
unsigned int vdd_cpucl1_od_lut_params[] = {
	2000000, 1,
};
unsigned int vdd_cpucl1_nm_lut_params[] = {
	1700000, 1,
};
unsigned int vdd_cpucl1_ud_lut_params[] = {
	1100000, 1,
};
unsigned int vdd_cpucl1_uud_lut_params[] = {
	500000, 0,
};
unsigned int vdd_aud_od_lut_params[] = {
	1500000, 9, 4, 0, 18, 9, 0, 0, 0,
};
unsigned int vdd_aud_nm_lut_params[] = {
	1200000, 7, 3, 1, 14, 7, 0, 0, 0,
};
unsigned int vdd_aud_sud_lut_params[] = {
	1200000, 7, 3, 1, 14, 7, 1, 2, 0,
};
unsigned int vdd_aud_ud_lut_params[] = {
	1200000, 7, 3, 1, 14, 7, 1, 0, 0,
};
unsigned int vdd_aud_uud_lut_params[] = {
	1200000, 7, 3, 1, 14, 7, 0, 2, 3,
};

/* SPECIAL VCLK -> LUT Parameter List */
unsigned int mux_clk_alive_i3c_pmic_uud_lut_params[] = {
	0, 1,
};
unsigned int clkcmu_alive_bus_uud_lut_params[] = {
	1, 0,
};
unsigned int div_clk_aud_pcmc_od_lut_params[] = {
	1, 7, 3, 0, 3, 0, 3, 0, 15, 0, 3, 0, 3, 0, 749, 1, 1, 3, 0, 3, 0, 1,
};
unsigned int div_clk_aud_pcmc_nm_lut_params[] = {
	1, 7, 3, 0, 3, 0, 3, 0, 15, 0, 3, 0, 3, 0, 749, 1, 1, 3, 0, 3, 0, 1,
};
unsigned int div_clk_aud_pcmc_ud_lut_params[] = {
	1, 7, 3, 0, 3, 0, 3, 0, 15, 0, 3, 0, 3, 0, 749, 1, 1, 3, 0, 3, 0, 1,
};
unsigned int div_clk_aud_pcmc_uud_lut_params[] = {
	1, 7, 3, 0, 3, 0, 3, 0, 15, 0, 3, 0, 3, 0, 749, 1, 1, 3, 0, 3, 0, 1,
};
unsigned int mux_busc_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_clkcmu_cmu_boost_uud_lut_params[] = {
	0,
};
unsigned int mux_clk_chub_timer_uud_lut_params[] = {
	0,
};
unsigned int mux_cmu_cmuref_uud_lut_params[] = {
	0,
};
unsigned int mux_core_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_cpucl0_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_dsu_cmuref_uud_lut_params[] = {
	0,
};
unsigned int mux_mif_cmuref_uud_lut_params[] = {
	1,
};
unsigned int mux_clk_usb_usb20drd_uud_lut_params[] = {
	0,
};
unsigned int clkcmu_usb_usb20drd_uud_lut_params[] = {
	0, 0,
};
unsigned int clkcmu_dpu_dsim_uud_lut_params[] = {
	4, 0,
};
unsigned int mux_clkcmu_peri_mmc_card_ud_lut_params[] = {
	2,
};
unsigned int mux_clkcmu_peri_mmc_card_uud_lut_params[] = {
	2,
};
unsigned int div_clk_alive_dbgcore_uart_uud_lut_params[] = {
	1, 1,
};
unsigned int div_clk_alive_usi0_uud_lut_params[] = {
	0, 1,
};
unsigned int div_clk_alive_i2c_uud_lut_params[] = {
	1, 1,
};
unsigned int mux_clkcmu_aud_cpu_uud_lut_params[] = {
	0,
};
unsigned int div_clk_aud_mclk_uud_lut_params[] = {
	1,
};
unsigned int div_clk_chub_usi0_uud_lut_params[] = {
	0, 1,
};
unsigned int clkcmu_chub_peri_uud_lut_params[] = {
	0, 1,
};
unsigned int div_clk_chub_usi1_uud_lut_params[] = {
	0, 1,
};
unsigned int div_clk_chub_usi2_uud_lut_params[] = {
	0, 1,
};
unsigned int div_clk_chub_usi3_uud_lut_params[] = {
	0, 1,
};
unsigned int div_clk_cmgp_usi0_uud_lut_params[] = {
	0, 1,
};
unsigned int clkcmu_cmgp_peri_uud_lut_params[] = {
	0, 1,
};
unsigned int div_clk_cmgp_usi4_uud_lut_params[] = {
	0, 1,
};
unsigned int div_clk_cmgp_i3c_uud_lut_params[] = {
	1, 1,
};
unsigned int div_clk_cmgp_usi1_nm_lut_params[] = {
	0,
};
unsigned int div_clk_cmgp_usi1_uud_lut_params[] = {
	0,
};
unsigned int div_clk_cmgp_usi2_nm_lut_params[] = {
	0,
};
unsigned int div_clk_cmgp_usi2_uud_lut_params[] = {
	0,
};
unsigned int div_clk_cmgp_usi3_nm_lut_params[] = {
	0,
};
unsigned int div_clk_cmgp_usi3_uud_lut_params[] = {
	0,
};
unsigned int clkcmu_cis_clk0_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk1_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk2_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk3_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk4_uud_lut_params[] = {
	3, 1,
};
unsigned int clkcmu_cis_clk5_uud_lut_params[] = {
	3, 1,
};
unsigned int div_clk_cpucl0_shortstop_sod_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_shortstop_od_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_shortstop_nm_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_shortstop_ud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_shortstop_sud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl0_shortstop_uud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_shortstop_sod_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_shortstop_od_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_shortstop_nm_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_shortstop_ud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_shortstop_uud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_htu_sod_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_htu_od_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_htu_nm_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_htu_ud_lut_params[] = {
	1,
};
unsigned int div_clk_cpucl1_htu_uud_lut_params[] = {
	1,
};
unsigned int div_clk_dsu_shortstop_sod_lut_params[] = {
	1,
};
unsigned int div_clk_dsu_shortstop_od_lut_params[] = {
	1,
};
unsigned int div_clk_dsu_shortstop_nm_lut_params[] = {
	1,
};
unsigned int div_clk_dsu_shortstop_ud_lut_params[] = {
	1,
};
unsigned int div_clk_dsu_shortstop_sud_lut_params[] = {
	1,
};
unsigned int div_clk_dsu_shortstop_uud_lut_params[] = {
	1,
};
unsigned int div_clk_cluster0_atclk_sod_lut_params[] = {
	3,
};
unsigned int div_clk_cluster0_atclk_od_lut_params[] = {
	3,
};
unsigned int div_clk_cluster0_atclk_nm_lut_params[] = {
	3,
};
unsigned int div_clk_cluster0_atclk_ud_lut_params[] = {
	3,
};
unsigned int div_clk_cluster0_atclk_sud_lut_params[] = {
	3,
};
unsigned int div_clk_cluster0_atclk_uud_lut_params[] = {
	3,
};
unsigned int div_clk_peri_usi00_usi_uud_lut_params[] = {
	0,
};
unsigned int mux_clkcmu_peri_ip_uud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi01_usi_uud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi02_usi_uud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi03_usi_uud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi04_usi_uud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_usi05_usi_uud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_uart_dbg_uud_lut_params[] = {
	1,
};
unsigned int div_clk_peri_usi06_usi_uud_lut_params[] = {
	0,
};
unsigned int div_clk_peri_400_lut_params[] = {
	0, 1,
};
unsigned int div_clk_peri_200_lut_params[] = {
	1, 1,
};
unsigned int div_clk_peri_133_lut_params[] = {
	2, 1,
};
unsigned int div_clk_peri_100_lut_params[] = {
	3, 1,
};
unsigned int div_clk_peri_80_lut_params[] = {
	4, 1,
};
unsigned int div_clk_peri_66_lut_params[] = {
	5, 1,
};
unsigned int div_clk_peri_57_lut_params[] = {
	6, 1,
};
unsigned int div_clk_peri_50_lut_params[] = {
	7, 1,
};
unsigned int div_clk_peri_44_lut_params[] = {
	8, 1,
};
unsigned int div_clk_peri_40_lut_params[] = {
	9, 1,
};
unsigned int div_clk_peri_36_lut_params[] = {
	10, 1,
};
unsigned int div_clk_peri_33_lut_params[] = {
	11, 1,
};
unsigned int div_clk_peri_30_lut_params[] = {
	12, 1,
};
unsigned int div_clk_peri_28_lut_params[] = {
	13, 1,
};
unsigned int div_clk_peri_25_lut_params[] = {
	15, 1,
};
unsigned int div_clk_peri_26_lut_params[] = {
	0, 0,
};
unsigned int div_clk_peri_13_lut_params[] = {
	1, 0,
};
unsigned int div_clk_peri_8_lut_params[] = {
	2, 0,
};
unsigned int div_clk_peri_6_lut_params[] = {
	3, 0,
};
unsigned int div_clk_peri_5_lut_params[] = {
	4, 0,
};
unsigned int div_clk_peri_4_lut_params[] = {
	5, 0,
};
unsigned int div_clk_peri_3_lut_params[] = {
	6, 0,
};
unsigned int div_vts_serial_lif_core_nm_lut_params[] = {
	0, 0, 1,
};
unsigned int div_vts_serial_lif_core_uud_lut_params[] = {
	0, 0, 0,
};
unsigned int clkcmu_chubvts_bus_uud_lut_params[] = {
	0, 1,
};
unsigned int mux_clkcmu_ap2gnss_uud_lut_params[] = {
	1,
};

/* COMMON VCLK -> LUT Parameter List */
unsigned int blk_cmu_nm_lut_params[] = {
	1333000, 936000, 3, 0, 0, 0, 0, 0, 1,
};
unsigned int blk_cmu_uud_lut_params[] = {
	1333000, 936000, 3, 0, 0, 0, 0, 0, 1,
};
unsigned int blk_s2d_uud_lut_params[] = {
	26000, 1,
};
unsigned int blk_alive_uud_lut_params[] = {
	1, 1, 0, 0,
};
unsigned int blk_aud_od_lut_params[] = {
	1, 1, 2, 0,
};
unsigned int blk_aud_nm_lut_params[] = {
	1, 1, 2, 0,
};
unsigned int blk_aud_ud_lut_params[] = {
	1, 1, 2, 0,
};
unsigned int blk_aud_uud_lut_params[] = {
	1, 1, 2, 0,
};
unsigned int blk_chub_uud_lut_params[] = {
	1, 1, 1, 0,
};
unsigned int blk_chubvts_nm_lut_params[] = {
	0, 0,
};
unsigned int blk_cmgp_uud_lut_params[] = {
	1, 1, 1,
};
unsigned int blk_core_uud_lut_params[] = {
	0, 1,
};
unsigned int blk_cpucl0_sod_lut_params[] = {
	0,
};
unsigned int blk_cpucl0_od_lut_params[] = {
	0,
};
unsigned int blk_cpucl0_nm_lut_params[] = {
	0,
};
unsigned int blk_cpucl0_ud_lut_params[] = {
	0,
};
unsigned int blk_cpucl0_sud_lut_params[] = {
	0,
};
unsigned int blk_cpucl0_uud_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_sod_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_od_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_nm_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_ud_lut_params[] = {
	0,
};
unsigned int blk_cpucl1_uud_lut_params[] = {
	0,
};
unsigned int blk_dsu_sod_lut_params[] = {
	0, 3, 3,
};
unsigned int blk_dsu_od_lut_params[] = {
	0, 3, 3,
};
unsigned int blk_dsu_nm_lut_params[] = {
	0, 3, 3,
};
unsigned int blk_dsu_ud_lut_params[] = {
	0, 3, 3,
};
unsigned int blk_dsu_sud_lut_params[] = {
	0, 3, 3,
};
unsigned int blk_dsu_uud_lut_params[] = {
	0, 3, 3,
};
unsigned int blk_usb_uud_lut_params[] = {
	0,
};
unsigned int blk_vts_nm_lut_params[] = {
	1, 0, 0, 0, 1, 0, 0, 1,
};
unsigned int blk_vts_uud_lut_params[] = {
	1, 0, 0, 0, 1, 0, 0, 0,
};
unsigned int blk_busc_uud_lut_params[] = {
	1,
};
unsigned int blk_cpucl0_glb_uud_lut_params[] = {
	1,
};
unsigned int blk_csis_uud_lut_params[] = {
	1,
};
unsigned int blk_dpu_uud_lut_params[] = {
	1,
};
unsigned int blk_g3d_uud_lut_params[] = {
	3,
};
unsigned int blk_isp_uud_lut_params[] = {
	1,
};
unsigned int blk_m2m_uud_lut_params[] = {
	1,
};
unsigned int blk_mcsc_uud_lut_params[] = {
	1,
};
unsigned int blk_mfc_uud_lut_params[] = {
	1,
};
unsigned int blk_npu0_ud_lut_params[] = {
	3,
};
unsigned int blk_npus_ud_lut_params[] = {
	3,
};
unsigned int blk_peri_uud_lut_params[] = {
	1,
};
unsigned int blk_taa_uud_lut_params[] = {
	1,
};
unsigned int blk_tnr_uud_lut_params[] = {
	1,
};
