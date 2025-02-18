/*****************************************************************************
 *
 * Copyright (c) 2012 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_QSFS_H__
#define __SLSI_QSFS_H__

#define SLSI_QSF_LOW_RX_CORE_GET_MASK  1
#define SLSI_QSF_MAIN_CORES_GET_MASK   0x0E
#define SLSI_QSF_MAIN_CORES_SHIFT 1
#define SLSI_QSF_SW_FEATURE_PNO_ID  1
#define SLSI_QSF_SW_FEATURE_PNO_LEN 1
#define SLSI_QSF_SW_FEATURE_TWT_ID  2
#define SLSI_QSF_SW_FEATURE_TWT_LEN 2
#define SLSI_QSF_SW_FEATURE_WIFI_OPTIMIZER_ID  3
#define SLSI_QSF_SW_FEATURE_WIFI_OPTIMIZER_LEN 1
#define SLSI_QSF_SW_FEATURE_SCHEDULED_PM_ID  4
#define SLSI_QSF_SW_FEATURE_SCHEDULED_PM_LEN 1
#define SLSI_QSF_SW_FEATURE_DELAYED_WAKEUP_ID  5
#define SLSI_QSF_SW_FEATURE_DELAYED_WAKEUP_LEN 1
#define SLSI_QSF_SW_FEATURE_RFC_8325_ID  6
#define SLSI_QSF_SW_FEATURE_RFC_8325_LEN 1
#define SLSI_QSF_NUM_ANTENNA_MAX 2
#define SLSI_QSF_SW_FEATURE_MHS_ID  7
#define SLSI_QSF_SW_FEATURE_MHS_LEN 2
#define SLSI_QSF_SW_FEATURE_ROAMING_LEN 4
#define SLSI_QSF_SW_FEATURE_NCHO_ID  9
#define SLSI_QSF_SW_FEATURE_ROAMING_ID  8
#define SLSI_QSF_SW_FEATURE_NCHO_LEN 2
#define SLSI_QSF_SW_FEATURE_ASSURANCE_ID  10
#define SLSI_QSF_SW_FEATURE_ASSURANCELEN  1
#define SLSI_QSF_SW_FEATURE_PCAP_FRAME_LOGGING_ID  11
#define SLSI_QSF_SW_FEATURE_PCAP_FRAME_LOGGING_LEN 1
#define SLSI_QSF_SW_FEATURE_SECURITY_ID  12
#define SLSI_QSF_SW_FEATURE_SECURITY_LEN 2
#define SLSI_QSF_SW_FEATURE_P2P_ID  13
#define SLSI_QSF_SW_FEATURE_P2P_LEN 6
#define SLSI_QSF_SW_FEATURE_BIG_DATA_ID  14
#define SLSI_QSF_SW_FEATURE_BIG_DATA_LEN 1
#define SLSI_QSF_SW_FEATURE_ASSURANCE_MASK  0x01

#define DUAL_BAND_CONCURRANCY_WITH_6G 2
#define DUAL_BAND_CONCURRANCY 1
#define SLSI_QSF_WIFI_STANDARD_MASK (BIT(0) | BIT(1) | BIT(2) | BIT(3))
#define SLSI_QSF_WIFI_STANDARD_POS 0
#define SLSI_QSF_SET_WIFI_STANDARD(hw_feature, wifi_standard) ((hw_feature) |= \
							      ((wifi_standard) << SLSI_QSF_WIFI_STANDARD_POS) & \
							      SLSI_QSF_WIFI_STANDARD_MASK)

#define SLSI_QSF_LOW_RX_CORE_POS    4
#define SLSI_QSF_LOW_RX_CORE_MASK   BIT(4)
#define SLSI_QSF_SET_LOW_PWR_RX_CORE(hw_feature, low_rx_core)   ((hw_feature) |= \
								((low_rx_core) << SLSI_QSF_LOW_RX_CORE_POS) & \
								SLSI_QSF_LOW_RX_CORE_MASK)

#define SLSI_QSF_MAIN_CORES_POS     5
#define SLSI_QSF_MAIN_CORES_MASK    (BIT(5) | BIT(6) | BIT(7))
#define SLSI_QSF_SET_NUMBER_OF_CORES(hw_feature, main_cores)     ((hw_feature) |= \
								 ((main_cores) << SLSI_QSF_MAIN_CORES_POS) & \
								 SLSI_QSF_MAIN_CORES_MASK)

#define SLSI_QSF_SUPP_CONC_MODE_POS    8
#define SLSI_QSF_SUPP_CONC_MODE_MASK   (BIT(8) | BIT(9) | BIT(10))
#define SLSI_QSF_SET_CONCURRENCY_MODES(hw_feature, supp_conc_mode)  ((hw_feature) |= \
								    ((supp_conc_mode) << SLSI_QSF_SUPP_CONC_MODE_POS) & \
								    SLSI_QSF_SUPP_CONC_MODE_MASK)

#define SLSI_QSF_ANTENNA_POS    11
#define SLSI_QSF_ANTENNA_MASK   (BIT(11) | BIT(12) | BIT(13))
#define SLSI_QSF_SET_NUM_ANTENNA(hw_feature, antenna)   ((hw_feature) |= \
							((antenna) << SLSI_QSF_ANTENNA_POS) & SLSI_QSF_ANTENNA_MASK)

#define SLSI_QSF_HW_FEATURE_LEN_POS  16
#define SLSI_QSF_HW_FEATURE_LEN_MASK 0xFFFF0000

#define SLSI_SUPPORTED_TX_MCS_PRESENT_AND_TXRX_MCS_NOT_EQUAL(caps)   (caps[12] & (BIT(0) | BIT(1)))
#define SLSI_GET_NSS_FROM_HT_CAPS(caps)     ((caps[12] & (BIT(2) | BIT(3))) >> 2)

#define SLSI_MAX_NSS_NOT_SUPPORTED 3
#define SLSI_MAX_MCS_8NSS(mcs_map)  (((mcs_map) & 0xC000) >> 14)
#define SLSI_MAX_MCS_7NSS(mcs_map)  (((mcs_map) & 0x3000) >> 12)
#define SLSI_MAX_MCS_6NSS(mcs_map)  (((mcs_map) & 0x0C00) >> 10)
#define SLSI_MAX_MCS_5NSS(mcs_map)  (((mcs_map) & 0x0300) >> 8)
#define SLSI_MAX_MCS_4NSS(mcs_map)  (((mcs_map) & 0x00C0) >> 6)
#define SLSI_MAX_MCS_3NSS(mcs_map)  (((mcs_map) & 0x0030) >> 4)
#define SLSI_MAX_MCS_2NSS(mcs_map)  (((mcs_map) & 0x000C) >> 2)
#define SLSI_MAX_MCS_1NSS(mcs_map)  ((mcs_map) & 0x0003)

#define SLSI_GET_HE_RX_80MHZ_MCS_MAP(caps)    SLSI_BUFF_LE_TO_U16((caps) + 17)
#define SLSI_GET_HE_TX_80MHZ_MCS_MAP(caps)    SLSI_BUFF_LE_TO_U16((caps) + 19)
#define SLSI_GET_HE_RX_160MHZ_MCS_MAP(caps)   SLSI_BUFF_LE_TO_U16((caps) + 21)
#define SLSI_GET_HE_TX_160MHZ_MCS_MAP(caps)   SLSI_BUFF_LE_TO_U16((caps) + 23)
#define SLSI_GET_VHT_RX_MCS_MAP(caps)         SLSI_BUFF_LE_TO_U16((caps) + 4)
#define SLSI_GET_VHT_TX_MCS_MAP(caps)         SLSI_BUFF_LE_TO_U16((caps) + 8)

#define SLSI_GET_PNO_STATE_IN_ASSOC(misc_features_activated) ((misc_features_activated[1] & BIT(2)) >> 2)
#define SLSI_PNO_ENABLED            BIT(0)
#define SLSI_PNO_UNASSOIATED_ENABED BIT(1)
#define SLSI_PNO_ASSOIATED_ENABED   BIT(2)
#define SLSI_TWT_ENABLED            BIT(0)
#define SLSI_GET_TWT_REQ(twt_control)       (((twt_control) & BIT(2)) >> 2)
#define SLSI_GET_TWT_BROADCAST(twt_control) ((twt_control) & BIT(0))
#define SLSI_GET_TWT_FLEXIBLE(twt_control)  (((twt_control) & BIT(3)) >> 3)
#define SLSI_GET_TWT_MIN_SERVICE_PERIOD(misc_features_activated) (misc_features_activated[2] & (BIT(0) | BIT(1)))
#define SLSI_GET_TWT_min_sleep_period(misc_features_activated) ((misc_features_activated[2] & (BIT(2) | BIT(3))) >> 2)
#define SLSI_TWT_REQ_SUPPORTED         BIT(1)
#define SLSI_TWT_BROADCAST_SUPPORTED    BIT(2)
#define SLSI_TWT_FLEXIBLE_SUPPORTED    BIT(3)
#define SLSI_SET_TWT_MIN_SERVICE_PERIOD(twt, min_service_period) ((twt) |= ((min_service_period) << 4))
#define SLSI_SET_TWTMIN_SLEEP_PERIOD(twt, min_sleep_period)      ((twt) |= ((min_sleep_period) << 6))
#define SLSI_GET_WIFI_OPTIMIZER_SUPPORT(misc_features_activated) (misc_features_activated[3] & BIT(0))
#define SLSI_GET_DYNAMIC_DWELL_CONTROL(misc_features_activated)  ((misc_features_activated[3] & BIT(1)) >> 1)
#define SLSI_GET_ENHANCED_PASSIVE_SCAN(misc_features_activated)  ((misc_features_activated[3] & BIT(2)) >> 2)

#define SLSI_WIFI_OPTIMIZER_SUPPORTED BIT(0)
#define SLSI_DYNAMIC_DWELL_CONTROL_SUPPORTED  BIT(1)
#define SLSI_ENHANCED_PASSIVE_SCAN_SUPPORDED  BIT(2)
#define SLSI_GET_SCHED_PM_SUPPORT(misc_features_activated)  (misc_features_activated[4] & BIT(0))
#define SLSI_SCHED_PM_ENABLED  BIT(0)
#define SLSI_GET_DELAYED_WAKEUP_SUPPORT(misc_features_activated)     (misc_features_activated[5] & BIT(0))
#define SLSI_DELAYED_WAKEUP_ENABLED BIT(0)
#define SLSI_GET_DUAL_IFACE_SUPPORT(misc_features_activated)  (misc_features_activated[6] & BIT(0))
#define SLSI_GET_SUPPORT_5G(misc_features_activated)          ((misc_features_activated[6] & BIT(1)) >> 1)
#define SLSI_GET_SUPPORT_6G(misc_features_activated)          ((misc_features_activated[6] & BIT(2)) >> 2)
#define SLSI_DUAL_IFACE_ENABLED BIT(0)
#define SLSI_5G_SUPPORT         BIT(1)
#define SLSI_6G_SUPPORT         BIT(2)
#define SLSI_SET_MAX_CLIENT(max_cli)   ((max_cli) << 3)
#define SLSI_COUNTRY_CODE_SET_API_SUPPORTED  BIT(7)
#define SLSI_QSF_VALID_CHAN_API_SUPPORTED    BIT(8)
#define SLSI_HE_ENABLED         BIT(9)
#define SLSI_WPA3_ENABLED       BIT(10)
#define SLSI_GET_ROAMING_MAJOR_NUMBER(appendix_versions)                ((appendix_versions) & 0x000000FF)
#define SLSI_GET_ROAMING_MINOR_NUMBER(appendix_versions)                (((appendix_versions) & 0x0000FF00) >> 8)
#define SLSI_GET_HIGH_CHAN_UTILIZATON_TRIGGER(misc_features_activated)  (misc_features_activated[7] & BIT(0))
#define SLSI_GET_EMERGENCY_ROAMING_TRIGGER(misc_features_activated)     ((misc_features_activated[7] & BIT(1)) >> 1)
#define SLSI_GET_BTM_ROAMING_TRIGGER(misc_features_activated)           ((misc_features_activated[7] & BIT(2)) >> 2)
#define SLSI_GET_IDLE_ROAMING_TRIGGER(misc_features_activated)          ((misc_features_activated[7] & BIT(3)) >> 3)
#define SLSI_GET_WTC_ROAMING_TRIGGER(misc_features_activated)           ((misc_features_activated[7] & BIT(4)) >> 4)
#define SLSI_GET_BT_COEX_ROAMING(misc_features_activated)               ((misc_features_activated[7] & BIT(5)) >> 5)
#define SLSI_GET_ROAMING_BETWEEN_WPA_AND_WPA2(misc_features_activated)  ((misc_features_activated[7] & BIT(6)) >> 6)
#define SLSI_GET_MANAGE_CHAN_LIST_API(misc_features_activated)          ((misc_features_activated[7] & BIT(7)) >> 7)
#define SLSI_GET_ADAPTIVE_11R(misc_features_activated)                  (misc_features_activated[8] & BIT(0))
#define SLSI_GET_ROAMING_CTRL_API_1_2(misc_features_activated)          ((misc_features_activated[8] & BIT(1)) >> 1)
#define SLSI_GET_ROAMING_CTRL_API_3_4(misc_features_activated)          ((misc_features_activated[8] & BIT(2)) >> 2)
#define SLSI_GET_ROAMING_CTRL_API_5(misc_features_activated)            ((misc_features_activated[8] & BIT(3)) >> 3)
#define SLSI_HIGH_CHAN_UTILIZATON_TRIGGER BIT(16)
#define SLSI_EMERGENCY_ROAMING_TRIGGER    BIT(17)
#define SLSI_BTM_ROAMING_TRIGGER          BIT(18)
#define SLSI_IDLE_ROAMING_TRIGGER         BIT(19)
#define SLSI_WTC_ROAMING_TRIGGER          BIT(20)
#define SLSI_BT_COEX_ROAMING              BIT(21)
#define SLSI_ROAMING_BETWEEN_WPA_AND_WPA2 BIT(22)
#define SLSI_MANAGE_CHAN_LIST_API         BIT(23)

#define SLSI_ADAPTIVE_11R                 BIT(24)
#define SLSI_ROAMING_CTRL_API_1_2         BIT(25)
#define SLSI_ROAMING_CTRL_API_3_4         BIT(26)
#define SLSI_ROAMING_CTRL_API_5           BIT(27)
#define SLSI_GET_NCHO_MAJOR_NUMBER(appendix_versions)         (((appendix_versions) & 0x00FF0000) >> 16)
#define SLSI_GET_NCHO_MINOR_NUMBER(appendix_versions)         (((appendix_versions) & 0xFF000000) >> 24)
#define SLSI_GET_SUPPORT_MGMT_FRAMES(misc_features_activated) (misc_features_activated[10] & BIT(0))
#define SLSI_GET_SUPPORT_CTRL_FRAMES(misc_features_activated) ((misc_features_activated[10] & BIT(1)) >> 1)
#define SLSI_GET_SUPPORT_DATA_FRAMES(misc_features_activated) ((misc_features_activated[10] & BIT(2)) >> 2)
#define SLSI_MGMT_FRAME_ENABLED BIT(0)
#define SLSI_CTRL_FRAME_ENABLED BIT(1)
#define SLSI_DATA_FRAME_ENABLED BIT(2)
#define SLSI_SUPPORT_SAE_H2E        BIT(0)
#define SLSI_SUPPORT_SAE_FT         BIT(1)
#define SLSI_SUPPORT_SUITE_B        BIT(2)
#define SLSI_SUPPORT_SUITE_B_192    BIT(3)
#define SLSI_SUPPORT_SHA_256        BIT(4)
#define SLSI_SUPPORT_SHA_384        BIT(5)
#define SLSI_SUPPORT_SHA_256_FT     BIT(6)
#define SLSI_SUPPORT_SHA_384_FT     BIT(7)
#define SLSI_SUPPORT_OWE            BIT(8)
#define SLSI_QSF_NAN_SUPPORTED   BIT(0)
#define SLSI_QSF_TDLS_SUPPORTED  BIT(1)
#define SLSI_QSF_6E_SUPPORTED    BIT(2)
#define SLSI_QSF_P2P_LISTEN_OFFLOADING_SUPPORTED BIT(3)
#define SLSI_QSF_NOA_PS_P2P_SUPPORTED            BIT(4)
#define SLSI_QSF_TDLS_OFFCHANNEL_SUPPORTED       BIT(5)
#define SLSI_QSF_TDLS_CAPA_ENHANCE_SUPPORTED     BIT(6)
#define SLSI_QSF_TDLS_PEER_UAPSD_SUPPORTED  BIT(7)
#define SLSI_GET_MAX_NAN_NDPS(misc_features_activated)                (misc_features_activated[14] & 0xF0)
#define SLSI_QSF_STA_P2P_SUPPORTED          BIT(0)
#define SLSI_QSF_STA_SOFTAP_SUPPORTED       BIT(1)
#define SLSI_QSF_STA_NAN_SUPPORTED          BIT(2)
#define SLSI_QSF_STA_TDLS_SUPPORTED         BIT(3)
#define SLSI_QSF_STA_SOFTAP_P2P_SUPPORTED   BIT(4)
#define SLSI_QSF_STA_SOFTAP_NAN_SUPPORTED   BIT(5)
#define SLSI_QSF_STA_P2P_P2P_SUPPORTED      BIT(6)
#define SLSI_QSF_STA_P2P_NAN_SUPPORTED      BIT(7)
#define SLSI_QSF_STA_P2P_TDLS_SUPPORTED         BIT(0)
#define SLSI_QSF_STA_SOFTAP_TDLS_SUPPORTED      BIT(1)
#define SLSI_QSF_STA_NAN_TDLS_SUPPORTED         BIT(2)
#define SLSI_QSF_STA_SOFTAP_P2P_TDLS_SUPPORTED  BIT(3)
#define SLSI_QSF_STA_SOFTAP_NAN_TDLS_SUPPORTED  BIT(4)
#define SLSI_QSF_STA_P2P_P2P_TDLS_SUPPORTED     BIT(5)
#define SLSI_QSF_STA_P2P_NAN_TDLS_SUPPORTED     BIT(6)
#define SLSI_GET_STANDARD_NAN_6E_SUPPORT(misc_features_activated)     (misc_features_activated[17] & BIT(0))
#define SLSI_GET_SAMSUNG_NAN_6E_SUPPORT(misc_features_activated)      ((misc_features_activated[17] & BIT(1)) >> 1)
#define SLSI_GET_NAN_VERSION(misc_features_activated)                 ((misc_features_activated[17] & (BIT(3) |\
								       BIT(4) | BIT(5))) >> 3)
#define SLSI_QSF_STANDARD_NAN_6E_SUPPORTED BIT(0)
#define SLSI_QSF_SAMSUNG_NAN_6E_SUPPORTED  BIT(1)
#define SLSI_QSF_NAN_FAST_CONNECT_SUPPORTED        BIT(2)
#define SLSI_QSF_NAN_VERSION_SET(nan_version)    ((nan_version) << 3)
#define SLSI_QSF_BSSI_INFO_API_SUPP_ENABLED      BIT(0)
#define SLSI_QSF_ASSOC_REJECT_INFO_API_ENABLED   BIT(1)
#define SLSI_QSF_STA_INFO_API_SUPP_ENABLED       BIT(2)

#define SLSI_QSF_SECURITY_FEATURE_MASK  (SLSI_SUPPORT_SAE_H2E | SLSI_SUPPORT_SAE_FT | SLSI_SUPPORT_SUITE_B |\
					 SLSI_SUPPORT_SUITE_B_192 | SLSI_SUPPORT_SHA_256 |\
					 SLSI_SUPPORT_SHA_384 | SLSI_SUPPORT_SHA_256_FT |\
					 SLSI_SUPPORT_SHA_384_FT | SLSI_SUPPORT_OWE)
#define SLSI_QSF_P2P_FEATURE_1ST_BYTE_MASK  (SLSI_QSF_P2P_LISTEN_OFFLOADING_SUPPORTED |\
					     SLSI_QSF_NOA_PS_P2P_SUPPORTED |\
					     SLSI_QSF_TDLS_OFFCHANNEL_SUPPORTED |\
					     SLSI_QSF_TDLS_CAPA_ENHANCE_SUPPORTED)
#define SLSI_QSF_P2P_FEATURE_3RD_BYTE_MASK  (SLSI_QSF_STA_P2P_SUPPORTED | SLSI_QSF_STA_SOFTAP_SUPPORTED |\
					     SLSI_QSF_STA_NAN_SUPPORTED | SLSI_QSF_STA_TDLS_SUPPORTED |\
					     SLSI_QSF_STA_SOFTAP_P2P_SUPPORTED |\
					     SLSI_QSF_STA_SOFTAP_NAN_SUPPORTED |\
					     SLSI_QSF_STA_P2P_P2P_SUPPORTED |\
					     SLSI_QSF_STA_P2P_NAN_SUPPORTED)
#define SLSI_QSF_P2P_FEATURE_4TH_BYTE_MASK  (SLSI_QSF_STA_P2P_TDLS_SUPPORTED |\
					     SLSI_QSF_STA_SOFTAP_TDLS_SUPPORTED |\
					     SLSI_QSF_STA_NAN_TDLS_SUPPORTED |\
					     SLSI_QSF_STA_SOFTAP_P2P_TDLS_SUPPORTED |\
					     SLSI_QSF_STA_SOFTAP_NAN_TDLS_SUPPORTED |\
					     SLSI_QSF_STA_P2P_P2P_TDLS_SUPPORTED |\
					     SLSI_QSF_STA_P2P_NAN_TDLS_SUPPORTED)
#define SLSI_MAX_TDLS_LINK (4)
#define SLSI_WIFI_5                   1
#define SLSI_WIFI_4                   0
#define SLSI_QSF_FEATURE_VER_LEN      4
#define SLSI_QSF_SOLUTION_PROVIDER    3
#define SLSI_QSF_SW_FEATURE_CHAR_LEN  4
#define SLSI_QSF_WIFI_FEATURE_VERSION 1
#define SLSI_QSF_WIFI_HCF_FILE_NAME   "wlan_sw.hcf"
#define SLSI_HCF_HEADER_LEN           8

struct slsi_qsf_mib_data {
	bool he_active;
	bool twt_active;
	bool he_softap_active;
	bool wpa3_active;
	bool tdls_active;
	bool tdls_peer_uapsd_active;
	bool nan_fast_connect;
	u16 max_tdls_cli;
	u8 ht_caps[21];
	u8 vht_caps[12];
	u8 he_caps[32];
	u8 misc_features_activated[18];
	u32 appendix_versions;
	u32 twt_control;
};

void slsi_qsf_init(struct slsi_dev *sdev);
void slsi_qsf_deinit(void);
void slsi_get_qsfs_feature_set(struct slsi_dev *sdev);

#endif
