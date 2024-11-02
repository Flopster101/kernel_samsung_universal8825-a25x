/*****************************************************************************
 *
 * Copyright (c) 2012 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/fs.h>

#include "mgt.h"
#include "dev.h"
#include "debug.h"
#include "qsfs.h"
#include "mib.h"
#include "mlme.h"

static char *sol_name = "SLS";
module_param(sol_name, charp, 0444);
MODULE_PARM_DESC(sol_name, "Solution Provider Name");

static ssize_t sysfs_show_qsf(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
struct kobj_attribute qsf_attr = __ATTR(feature, 0400, sysfs_show_qsf, NULL);
static struct kobject *wifi_kobj_ref;

static int slsi_qsf_get_wifi_fw_feature_version(struct slsi_dev *sdev, u32 *fw_ver)
{
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_value *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_QSFS_VERION, { 0, 0 } },};
	const struct firmware *e = NULL;
	int r = 0;

	r = mx140_file_request_conf(sdev->maxwell_core, &e, "wlan", SLSI_QSF_WIFI_HCF_FILE_NAME);
	if (r || !e) {
		SLSI_ERR(sdev, "HCF file read failed as file %s is NOT found\n", SLSI_QSF_WIFI_HCF_FILE_NAME);
		return -1;
	}
	mibrsp.dataLength = e->size - SLSI_HCF_HEADER_LEN;
	mibrsp.data = (u8 *)e->data + SLSI_HCF_HEADER_LEN;
	values = slsi_mib_decode_get_list(&mibrsp, ARRAY_SIZE(get_values), get_values);
	if (!values) {
		SLSI_ERR(sdev, "QSFS mib decode failed returned values as NULL\n");
		mx140_file_release_conf(sdev->maxwell_core, e);
		return -1;
	}

	*fw_ver = values[0].u.uintValue;
	kfree(values);
	mx140_file_release_conf(sdev->maxwell_core, e);
	return 0;
}

void slsi_qsf_init(struct slsi_dev *sdev)
{
	int r = 0;

	wifi_kobj_ref = mxman_wifi_kobject_ref_get();
	pr_info("wifi_kobj_ref: 0x%p\n", wifi_kobj_ref);

	if (!wifi_kobj_ref)
		return;

	r = sysfs_create_file(wifi_kobj_ref, &qsf_attr.attr);
	if (r) {
		pr_err("Can't create /sys/wifi/feature\n");
		mxman_wifi_kobject_ref_put();
	}
}

void slsi_qsf_deinit(void)
{
	if (!wifi_kobj_ref)
		return;

	sysfs_remove_file(wifi_kobj_ref, &qsf_attr.attr);
	mxman_wifi_kobject_ref_put();
}

static u8 slsi_get_wifi_standard(struct slsi_dev *sdev)
{
	if (sdev->fw_vht_enabled)
		return SLSI_WIFI_5;

	if (sdev->fw_ht_enabled)
		return SLSI_WIFI_4;
	return 0xFF;
}

static u32 slsi_get_antenna_from_ht_caps(u8 *caps)
{
	u8 max_nss = 0;

	if (SLSI_SUPPORTED_TX_MCS_PRESENT_AND_TXRX_MCS_NOT_EQUAL(caps) == 3)
		max_nss = SLSI_GET_NSS_FROM_HT_CAPS(caps);
	else
		return 1;

	switch (max_nss) {
	case 0:
		return 1;
	case 1:
		return 2;
	case 2:
		return 3;
	case 3:
		return 4;
	default:
		return 1;
	}
}

static u8 slsi_get_nss_from_mcs_nss_map(u16 mcs_map)
{
	if (SLSI_MAX_MCS_8NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 8;
	if (SLSI_MAX_MCS_7NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 7;
	if (SLSI_MAX_MCS_6NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 6;
	if (SLSI_MAX_MCS_5NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 5;
	if (SLSI_MAX_MCS_4NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 4;
	if (SLSI_MAX_MCS_3NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 3;
	if (SLSI_MAX_MCS_2NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 2;
	if (SLSI_MAX_MCS_1NSS(mcs_map) != SLSI_MAX_NSS_NOT_SUPPORTED)
		return 1;

	return 1;
}

static u32 slsi_get_antenna_from_vht_caps(u8 *caps)
{
	u16 rx_vht_mcs_map = 0, tx_vht_mcs_map = 0;
	u8 tx_max_nss = 0, rx_max_nss = 0;

	rx_vht_mcs_map = SLSI_GET_VHT_RX_MCS_MAP(caps);
	tx_vht_mcs_map = SLSI_GET_VHT_TX_MCS_MAP(caps);

	tx_max_nss = slsi_get_nss_from_mcs_nss_map(tx_vht_mcs_map);
	rx_max_nss = slsi_get_nss_from_mcs_nss_map(rx_vht_mcs_map);

	if (tx_max_nss == 0 && rx_max_nss == 0)
		return 1;
	return max(tx_max_nss, rx_max_nss);
}

static u32 slsi_qsf_encode_hw_feature(struct slsi_dev *sdev, u8 *misc_features_activated,
				      bool he_active, char *buf, u32 bytes, u32 buf_size,
				      u8 *he_caps, u8 *vht_caps, u8 *ht_caps)
{
	u8 wifi_standard = 0;
	u32 hw_feature = 0;
	u8 low_rx_core = 0;
	u32 main_cores = 0;
	u32 antenna = 0;
	u32 hw_feature_len = 4;

	wifi_standard = slsi_get_wifi_standard(sdev);
	if (wifi_standard <= SLSI_WIFI_5) {
		SLSI_QSF_SET_WIFI_STANDARD(hw_feature, wifi_standard);
	} else {
		SLSI_ERR(sdev, "Error while getting wifi_standard, %d\n", wifi_standard);
		return bytes;
	}
	low_rx_core = misc_features_activated[0] & SLSI_QSF_LOW_RX_CORE_GET_MASK;
	if (low_rx_core <= 1) {
		SLSI_QSF_SET_LOW_PWR_RX_CORE(hw_feature, low_rx_core);
	} else {
		SLSI_ERR(sdev, "Error in getting low_rx_core, %d\n", low_rx_core);
		return bytes;
	}
	main_cores = (misc_features_activated[0] & SLSI_QSF_MAIN_CORES_GET_MASK) >> SLSI_QSF_MAIN_CORES_SHIFT;
	if (main_cores <= 2) {
		SLSI_QSF_SET_NUMBER_OF_CORES(hw_feature, main_cores);
	} else {
		SLSI_ERR(sdev, "Error in getting main_cores, %d\n", main_cores);
		return bytes;
	}

	switch (wifi_standard) {
	case SLSI_WIFI_5:
		antenna = slsi_get_antenna_from_vht_caps(vht_caps);
		break;
	case SLSI_WIFI_4:
		antenna = slsi_get_antenna_from_ht_caps(ht_caps);
		break;
	default:
		antenna = 1;
	}
	if (antenna <= SLSI_QSF_NUM_ANTENNA_MAX) {
		SLSI_QSF_SET_NUM_ANTENNA(hw_feature, antenna);
	} else {
		SLSI_ERR(sdev, "Error in getting antenna, %d\n", antenna);
		return bytes;
	}

	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%04X", hw_feature_len, cpu_to_le16(hw_feature));
	return bytes;
}

static u32 slsi_qsf_encode_sw_feature_1(struct slsi_dev *sdev, u8 *misc_features_activated,
					u32 twt_control, bool twt_active,
					char *buf, u32 bytes, u32 buf_size)
{
	u8 wifi_optimizer = 0, scheduled_pm = 0, delayed_wakeup = 0, rfc_8325 = 0, pno = 0;
	u32 pno_enabled = 1, pno_in_un_assocociated = 1, twt = 0;

	u32 twt_broadcast = SLSI_GET_TWT_BROADCAST(twt_control);
	u32 twt_req = SLSI_GET_TWT_REQ(twt_control);
	u32 twt_flexible = SLSI_GET_TWT_FLEXIBLE(twt_control);
	u32 min_service_period = SLSI_GET_TWT_MIN_SERVICE_PERIOD(misc_features_activated);
	u32 min_sleep_period = SLSI_GET_TWT_min_sleep_period(misc_features_activated);
	u32 pno_in_associated = SLSI_GET_PNO_STATE_IN_ASSOC(misc_features_activated);
	u32 wifi_optimizer_support = SLSI_GET_WIFI_OPTIMIZER_SUPPORT(misc_features_activated);
	u32 dynamic_dwell_control = SLSI_GET_DYNAMIC_DWELL_CONTROL(misc_features_activated);
	u32 enhanced_passive_scan = SLSI_GET_ENHANCED_PASSIVE_SCAN(misc_features_activated);
	u32 sched_pm_support = SLSI_GET_SCHED_PM_SUPPORT(misc_features_activated);
	u32 delayed_wakeup_supp = SLSI_GET_DELAYED_WAKEUP_SUPPORT(misc_features_activated);

	if (pno_enabled)
		pno |= SLSI_PNO_ENABLED;
	if (pno_in_un_assocociated)
		pno |= SLSI_PNO_UNASSOIATED_ENABED;
	if (pno_in_associated)
		pno |= SLSI_PNO_ASSOIATED_ENABED;
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_PNO_ID,
			   SLSI_QSF_SW_FEATURE_PNO_LEN, pno);

	if (twt_active)
		twt |= SLSI_TWT_ENABLED;

	if (twt_req)
		twt |= SLSI_TWT_REQ_SUPPORTED;
	if (twt_broadcast)
		twt |= SLSI_TWT_BROADCAST_SUPPORTED;
	if (twt_flexible)
		twt |= SLSI_TWT_FLEXIBLE_SUPPORTED;
	SLSI_SET_TWT_MIN_SERVICE_PERIOD(twt, min_service_period);
	SLSI_SET_TWTMIN_SLEEP_PERIOD(twt, min_sleep_period);
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%04X", SLSI_QSF_SW_FEATURE_TWT_ID,
			   SLSI_QSF_SW_FEATURE_TWT_LEN, cpu_to_le16(twt));

	if (wifi_optimizer_support)
		wifi_optimizer |= SLSI_WIFI_OPTIMIZER_SUPPORTED;
	if (dynamic_dwell_control)
		wifi_optimizer |= SLSI_DYNAMIC_DWELL_CONTROL_SUPPORTED;
	if (enhanced_passive_scan)
		wifi_optimizer |= SLSI_ENHANCED_PASSIVE_SCAN_SUPPORDED;
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_WIFI_OPTIMIZER_ID,
			   SLSI_QSF_SW_FEATURE_WIFI_OPTIMIZER_LEN, wifi_optimizer);

	if (sched_pm_support)
		scheduled_pm = SLSI_SCHED_PM_ENABLED;
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_SCHEDULED_PM_ID,
			   SLSI_QSF_SW_FEATURE_SCHEDULED_PM_LEN, scheduled_pm);
	if (delayed_wakeup_supp)
		delayed_wakeup = SLSI_DELAYED_WAKEUP_ENABLED;
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_DELAYED_WAKEUP_ID,
			   SLSI_QSF_SW_FEATURE_DELAYED_WAKEUP_LEN, delayed_wakeup);
#ifndef CONFIG_SCSC_USE_WMM_TOS
	rfc_8325 |= 0x01;
#endif
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_RFC_8325_ID,
			   SLSI_QSF_SW_FEATURE_RFC_8325_LEN, rfc_8325);
	return bytes;
}

static u32 slsi_qsf_encode_sw_feature_2(struct slsi_dev *sdev, u8 *misc_features_activated,
					bool he_softap_active, bool wpa3_active, u32 appendix_versions,
					char *buf, u32 bytes, u32 buf_size)
{
#ifdef SCSC_SEP_VERSION
	u16 max_cli = sdev->softap_max_client;
#endif
	u32 mhs = 0, roaming = 0, country_code_set_hal_api = 1, get_valid_chan_hal_api = 1;

	u32 dual_interface = SLSI_GET_DUAL_IFACE_SUPPORT(misc_features_activated);
	u32 support_5g = SLSI_GET_SUPPORT_5G(misc_features_activated);
	u32 support_6g = SLSI_GET_SUPPORT_6G(misc_features_activated);
	u32 high_chan_utilization_trigger = SLSI_GET_HIGH_CHAN_UTILIZATON_TRIGGER(misc_features_activated);
	u32 emergency_roaming_trigger = SLSI_GET_EMERGENCY_ROAMING_TRIGGER(misc_features_activated);
	u32 btm_roaming_trigger = SLSI_GET_BTM_ROAMING_TRIGGER(misc_features_activated);
	u32 roaming_major_number = SLSI_GET_ROAMING_MAJOR_NUMBER(appendix_versions);
	u32 roaming_minor_number = SLSI_GET_ROAMING_MINOR_NUMBER(appendix_versions);
	u32 idle_roaming_trogger = SLSI_GET_IDLE_ROAMING_TRIGGER(misc_features_activated);
	u32 wtc_roaming_trigger = SLSI_GET_WTC_ROAMING_TRIGGER(misc_features_activated);
	u32 bt_coex_roaming_trigger = SLSI_GET_BT_COEX_ROAMING(misc_features_activated);
	u32 roaming_bwtween_wpa_and_wpa2 = SLSI_GET_ROAMING_BETWEEN_WPA_AND_WPA2(misc_features_activated);
	u32 roaming_ctrl_api_1_2 = SLSI_GET_ROAMING_CTRL_API_1_2(misc_features_activated);
	u32 roaming_ctrl_api_3_4 = SLSI_GET_ROAMING_CTRL_API_3_4(misc_features_activated);
	u32 roaming_ctrl_api_5 = SLSI_GET_ROAMING_CTRL_API_5(misc_features_activated);
	u32 manage_chan_list_api = SLSI_GET_MANAGE_CHAN_LIST_API(misc_features_activated);
	u32 adaptive_11r = SLSI_GET_ADAPTIVE_11R(misc_features_activated);

	if (dual_interface)
		mhs |= SLSI_DUAL_IFACE_ENABLED;
	if (support_5g)
		mhs |= SLSI_5G_SUPPORT;
	if (support_6g)
		mhs |= SLSI_6G_SUPPORT;
#ifdef SCSC_SEP_VERSION
	if (max_cli)
		mhs |= SLSI_SET_MAX_CLIENT(max_cli);
#endif
	if (country_code_set_hal_api)
		mhs |= SLSI_COUNTRY_CODE_SET_API_SUPPORTED;
	if (get_valid_chan_hal_api)
		mhs |= SLSI_QSF_VALID_CHAN_API_SUPPORTED;
	if (he_softap_active)
		mhs |= SLSI_HE_ENABLED;
	if (wpa3_active)
		mhs |= SLSI_WPA3_ENABLED;
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%04X", SLSI_QSF_SW_FEATURE_MHS_ID,
			   SLSI_QSF_SW_FEATURE_MHS_LEN, cpu_to_le16(mhs));

	roaming |= roaming_major_number;
	roaming |= roaming_minor_number << 8;

	if (high_chan_utilization_trigger)
		roaming |= SLSI_HIGH_CHAN_UTILIZATON_TRIGGER;
	if (emergency_roaming_trigger)
		roaming |= SLSI_EMERGENCY_ROAMING_TRIGGER;
	if (btm_roaming_trigger)
		roaming |= SLSI_BTM_ROAMING_TRIGGER;
	if (idle_roaming_trogger)
		roaming |= SLSI_IDLE_ROAMING_TRIGGER;
	if (wtc_roaming_trigger)
		roaming |= SLSI_WTC_ROAMING_TRIGGER;
	if (bt_coex_roaming_trigger)
		roaming |= SLSI_BT_COEX_ROAMING;
	if (roaming_bwtween_wpa_and_wpa2)
		roaming |= SLSI_ROAMING_BETWEEN_WPA_AND_WPA2;
	if (manage_chan_list_api)
		roaming |= SLSI_MANAGE_CHAN_LIST_API;
	if (adaptive_11r)
		roaming |= SLSI_ADAPTIVE_11R;
	if (roaming_ctrl_api_1_2)
		roaming |= SLSI_ROAMING_CTRL_API_1_2;
	if (roaming_ctrl_api_3_4)
		roaming |= SLSI_ROAMING_CTRL_API_3_4;
	if (roaming_ctrl_api_5)
		roaming |= SLSI_ROAMING_CTRL_API_5;

	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%08X", SLSI_QSF_SW_FEATURE_ROAMING_ID,
			   SLSI_QSF_SW_FEATURE_ROAMING_LEN, cpu_to_be32(roaming));
	return bytes;
}

static u32 slsi_qsf_encode_sw_feature_3(struct slsi_dev *sdev, u8 *misc_features_activated,
					u32 appendix_versions,
					char *buf, u32 bytes, u32 buf_size)
{
	u32 ncho = 0;
	u32 ncho_major = 0;
	u32 ncho_minor = 0;
	u8 pcap_frame_logging = 0;
	u16 security = 0;

	u32 mgmt_frame = SLSI_GET_SUPPORT_MGMT_FRAMES(misc_features_activated);
	u32 ctrl_frame = SLSI_GET_SUPPORT_CTRL_FRAMES(misc_features_activated);
	u32 data_frame = SLSI_GET_SUPPORT_DATA_FRAMES(misc_features_activated);
	u8 assurance = misc_features_activated[9] & SLSI_QSF_SW_FEATURE_ASSURANCE_MASK;

	ncho_major = SLSI_GET_NCHO_MAJOR_NUMBER(appendix_versions);
	ncho_minor = SLSI_GET_NCHO_MINOR_NUMBER(appendix_versions);

	ncho |= ncho_major;
	ncho |= ncho_minor << 8;

	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%04X", SLSI_QSF_SW_FEATURE_NCHO_ID,
			   SLSI_QSF_SW_FEATURE_NCHO_LEN, cpu_to_be16(ncho));
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_ASSURANCE_ID,
			   SLSI_QSF_SW_FEATURE_ASSURANCELEN, assurance);
	if (mgmt_frame)
		pcap_frame_logging |= SLSI_MGMT_FRAME_ENABLED;
	if (ctrl_frame)
		pcap_frame_logging |= SLSI_CTRL_FRAME_ENABLED;
	if (data_frame)
		pcap_frame_logging |= SLSI_DATA_FRAME_ENABLED;
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_PCAP_FRAME_LOGGING_ID,
			   SLSI_QSF_SW_FEATURE_PCAP_FRAME_LOGGING_LEN, pcap_frame_logging);

	security = (SLSI_BUFF_LE_TO_U16(misc_features_activated + 11) & SLSI_QSF_SECURITY_FEATURE_MASK);

	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%04X", SLSI_QSF_SW_FEATURE_SECURITY_ID,
			   SLSI_QSF_SW_FEATURE_SECURITY_LEN, cpu_to_be16(security));
	return bytes;
}

static u32 slsi_qsf_encode_sw_feature_4(struct slsi_dev *sdev, u8 *misc_features_activated,
					bool tdls_active, u16 max_tdls_cli,
					bool tdls_peer_uapsd_active, bool nan_fast_connect,
					char *buf, u32 bytes, u32 buf_size)
{
	u32 get_bssi_info_api_supp = 1, get_assoc_reject_info_api_supp = 1;
	u32 get_sta_info_api_supp = 1;
	u8 p2p[6] = {0};
	u8 big_data = 0;

	u32 max_nan_ndps = SLSI_GET_MAX_NAN_NDPS(misc_features_activated);

	u32 standard_nan_6e = SLSI_GET_STANDARD_NAN_6E_SUPPORT(misc_features_activated);
	u32 samsung_nan_6e = SLSI_GET_SAMSUNG_NAN_6E_SUPPORT(misc_features_activated);
	u32 nan_version = SLSI_GET_NAN_VERSION(misc_features_activated);

	if (sdev->nan_enabled)
		p2p[0] |= SLSI_QSF_NAN_SUPPORTED;
	if (tdls_active)
		p2p[0] |= SLSI_QSF_TDLS_SUPPORTED;
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	if (sdev->band_6g_supported)
		p2p[0] |= SLSI_QSF_6E_SUPPORTED;
#endif
	if (tdls_peer_uapsd_active)
		p2p[0] |= SLSI_QSF_TDLS_PEER_UAPSD_SUPPORTED;
	p2p[0] |= misc_features_activated[13] & SLSI_QSF_P2P_FEATURE_1ST_BYTE_MASK;

	if (max_tdls_cli)
		p2p[1] |= max_tdls_cli & 0xF;
	if (max_nan_ndps)
		p2p[1] |= max_nan_ndps;

	p2p[2] = misc_features_activated[15] & (SLSI_QSF_P2P_FEATURE_3RD_BYTE_MASK);

	p2p[3] = misc_features_activated[16] & SLSI_QSF_P2P_FEATURE_4TH_BYTE_MASK;
	if (standard_nan_6e)
		p2p[5] |= SLSI_QSF_STANDARD_NAN_6E_SUPPORTED;
	if (samsung_nan_6e)
		p2p[5] |= SLSI_QSF_SAMSUNG_NAN_6E_SUPPORTED;
	if (nan_fast_connect)
		p2p[5] |= SLSI_QSF_NAN_FAST_CONNECT_SUPPORTED;
	if (nan_version)
		p2p[5] |= SLSI_QSF_NAN_VERSION_SET(nan_version);

	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X%02X%02X%02X%02X%02X",
			   SLSI_QSF_SW_FEATURE_P2P_ID, SLSI_QSF_SW_FEATURE_P2P_LEN, p2p[0],
			   p2p[1], p2p[2], p2p[3], p2p[4], p2p[5]);

	if (get_bssi_info_api_supp)
		big_data |= SLSI_QSF_BSSI_INFO_API_SUPP_ENABLED;
	if (get_assoc_reject_info_api_supp)
		big_data |= SLSI_QSF_ASSOC_REJECT_INFO_API_ENABLED;
	if (get_sta_info_api_supp)
		big_data |= SLSI_QSF_STA_INFO_API_SUPP_ENABLED;
	bytes += scnprintf(buf + bytes, buf_size - bytes, "%02X%02X%02X", SLSI_QSF_SW_FEATURE_BIG_DATA_ID,
			   SLSI_QSF_SW_FEATURE_BIG_DATA_LEN, big_data);
	return bytes;
}

static inline void slsi_qsf_extract_bool_data(struct slsi_dev *sdev, struct slsi_mib_value *values,
					      int index, bool *val)
{
	if (values[index].type == SLSI_MIB_TYPE_UINT || values[index].type == SLSI_MIB_TYPE_BOOL)
		*val = values[index].u.boolValue;
	else
		SLSI_ERR(sdev, "Type mismatch for index: %d\n", index);
}

static int slsi_get_qsf_mib_data(struct slsi_dev *sdev, struct slsi_qsf_mib_data *qsf_data)
{
	struct slsi_mib_data mibrsp = { 0, NULL };
	struct slsi_mib_value *values = NULL;
	struct slsi_mib_get_entry get_values[] = { { SLSI_PSID_UNIFI_HE_ACTIVATED, { 0, 0 } },
						   { SLSI_PSID_UNIFI_TWT_ACTIVATED, {0, 0} },
						   { SLSI_PSID_UNIFI_WP_A3_ACTIVATED, {0, 0} },
						   { SLSI_PSID_UNIFI_TDLS_ACTIVATED, {0, 0} },
						   { SLSI_PSID_DOT11_TDLS_PEER_UAPSD_BUFFER_STA_ACTIVATED, {0, 0} },
						   { SLSI_PSID_UNIFI_HT_CAPABILITIES, {0, 0} },
						   { SLSI_PSID_UNIFI_VHT_CAPABILITIES, {0, 0} },
						   { SLSI_PSID_UNIFI_HE_CAPABILITIES, {0, 0} },
						   { SLSI_PSID_UNIFI_MISC_FEATURES_ACTIVATED, {0, 0} },
						   { SLSI_PSID_UNIFI_APPENDIX_VERSIONS, {0, 0} },
						   { SLSI_PSID_UNIFI_TWT_CONTROL_FLAGS, {0, 0} },
						 };

	mibrsp.dataLength = 32 * ARRAY_SIZE(get_values);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		return -ENOMEM;
	}

	values = slsi_read_mibs(sdev, NULL, get_values, ARRAY_SIZE(get_values), &mibrsp);
	if (!values) {
		kfree(mibrsp.data);
		SLSI_ERR(sdev, "Error in slsi_read_mibs\n");
		return -1;
	}

	qsf_data->max_tdls_cli = SLSI_MAX_TDLS_LINK;
	slsi_qsf_extract_bool_data(sdev, values, 0, &qsf_data->he_active);
	slsi_qsf_extract_bool_data(sdev, values, 1, &qsf_data->twt_active);
	slsi_qsf_extract_bool_data(sdev, values, 2, &qsf_data->wpa3_active);
	slsi_qsf_extract_bool_data(sdev, values, 3, &qsf_data->tdls_active);
	slsi_qsf_extract_bool_data(sdev, values, 4, &qsf_data->tdls_peer_uapsd_active);

	if (values[5].type == SLSI_MIB_TYPE_OCTET && values[5].u.octetValue.dataLength >= 21)
		memcpy(qsf_data->ht_caps, values[5].u.octetValue.data, 21);
	else
		SLSI_ERR(sdev, "invalid type or len for index: %d len:%d\n", 5,
			 values[5].u.octetValue.dataLength);
	if (values[6].type == SLSI_MIB_TYPE_OCTET && values[6].u.octetValue.dataLength >= 12)
		memcpy(qsf_data->vht_caps, values[6].u.octetValue.data, 12);
	else
		SLSI_ERR(sdev, "invalid type or len for index: %d len: %d\n", 6,
			 values[6].u.octetValue.dataLength);
	if (values[7].type == SLSI_MIB_TYPE_OCTET && values[7].u.octetValue.dataLength >= 28)
		memcpy(qsf_data->he_caps, values[7].u.octetValue.data, values[7].u.octetValue.dataLength);
	else
		SLSI_ERR(sdev, "invalid type or len for index: %d len: %d\n", 7,
			 values[7].u.octetValue.dataLength);
	if (values[8].type == SLSI_MIB_TYPE_OCTET && values[8].u.octetValue.dataLength >= 18)
		memcpy(qsf_data->misc_features_activated, values[8].u.octetValue.data, 18);
	else
		SLSI_ERR(sdev, "invalid type or len for index: %d len: %d\n", 8,
			 values[8].u.octetValue.dataLength);
	if (values[9].type == SLSI_MIB_TYPE_OCTET && values[9].u.octetValue.dataLength >= 4)
		memcpy(&qsf_data->appendix_versions, values[9].u.octetValue.data, 4);
	else
		SLSI_ERR(sdev, "invalid type or len for index: %d len: %d\n", 9,
			 values[9].u.octetValue.dataLength);

	SLSI_CHECK_TYPE(sdev, values[10].type, SLSI_MIB_TYPE_UINT);
	qsf_data->twt_control = values[10].u.uintValue;
	kfree(mibrsp.data);
	kfree(values);
	return 0;
}

void slsi_get_qsfs_feature_set(struct slsi_dev *sdev)
{
	int sw_feature_len = 0;
	int sw_feature_len_offset = 0;
	u32 pos = sdev->qsf_feature_set_len;
	int ret = 0;
	u32 buf_size = 128;
	struct slsi_qsf_mib_data qsf_data = {0};
	u8  *buf = sdev->qsfs_feature_set;
	char tmp_buf[5] = {0};

	ret = slsi_get_qsf_mib_data(sdev, &qsf_data);
	if (ret) {
		SLSI_ERR(sdev, "Error in getting thr mib data Error:%d\n", ret);
		return;
	}

	pos = slsi_qsf_encode_hw_feature(sdev, qsf_data.misc_features_activated, qsf_data.he_active,
					 buf, pos, buf_size, qsf_data.he_caps,
					 qsf_data.vht_caps, qsf_data.ht_caps);
	sw_feature_len_offset = pos;
	pos += SLSI_QSF_SW_FEATURE_CHAR_LEN;
	pos = slsi_qsf_encode_sw_feature_1(sdev, qsf_data.misc_features_activated, qsf_data.twt_control,
					   qsf_data.twt_active, buf, pos, buf_size);
	pos = slsi_qsf_encode_sw_feature_2(sdev, qsf_data.misc_features_activated, qsf_data.he_softap_active,
					   qsf_data.wpa3_active, qsf_data.appendix_versions,
					   buf, pos, buf_size);
	pos = slsi_qsf_encode_sw_feature_3(sdev, qsf_data.misc_features_activated, qsf_data.appendix_versions,
					   buf, pos, buf_size);
	pos = slsi_qsf_encode_sw_feature_4(sdev, qsf_data.misc_features_activated, qsf_data.tdls_active,
					   qsf_data.max_tdls_cli, qsf_data.tdls_peer_uapsd_active,
					   qsf_data.nan_fast_connect, buf, pos, buf_size);
	sw_feature_len = pos - (sw_feature_len_offset + SLSI_QSF_SW_FEATURE_CHAR_LEN);
	scnprintf(tmp_buf, 5, "%04X", sw_feature_len);
	memcpy(buf + sw_feature_len_offset, tmp_buf, SLSI_QSF_SW_FEATURE_CHAR_LEN);
	sdev->qsf_feature_set_len = pos;
}

static ssize_t sysfs_show_qsf(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct slsi_dev *sdev = slsi_get_sdev();
	int r = 0;
	u32 fw_feature_ver = 0;
	u32 wifi_feature_ver = 0;
	u32 pos = 0;
	u32 buf_size = sizeof(sdev->qsfs_feature_set);

	if (!sdev->qsf_feature_set_len) {
		r = slsi_qsf_get_wifi_fw_feature_version(sdev, &fw_feature_ver);
		SLSI_INFO(sdev, "fw_feature_ver: %d\n", fw_feature_ver);
		if (r < 0)
			SLSI_ERR(sdev, "QSFS Wifi FW feature version couldn't be identified\n");
		if (fw_feature_ver)
			wifi_feature_ver = fw_feature_ver + SLSI_QSF_WIFI_FEATURE_VERSION;

		pos += scnprintf(sdev->qsfs_feature_set, buf_size, "%04X", wifi_feature_ver);
		pos += scnprintf(sdev->qsfs_feature_set + pos, buf_size - pos, "%.3s", sol_name);
		sdev->qsf_feature_set_len = pos;
	}
	SLSI_INFO(sdev, "sysfs node for QSF is being read\n");
	memcpy(buf, sdev->qsfs_feature_set, sdev->qsf_feature_set_len);
	return sdev->qsf_feature_set_len;
}
