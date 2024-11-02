/*****************************************************************************
 *
 * Copyright (c) 2012 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __SLSI_LOG2US_H__
#define __SLSI_LOG2US_H__
#include <linux/types.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/netdevice.h>

#include "mib.h"
#include "dev.h"

struct slsi_dev;
#define SLSI_LOG2US_BURST 20
#define BUFF_SIZE 256

void slsi_conn_log2us_init(struct slsi_dev *sdev);
void slsi_conn_log2us_deinit(struct slsi_dev *sdev);
void slsi_conn_log2us_connecting(struct slsi_dev *sdev, struct net_device *dev, struct cfg80211_connect_params *sme);

void slsi_conn_log2us_connecting_fail(struct slsi_dev *sdev, struct net_device *dev,
				      const unsigned char *bssid,
				      int freq, int reason);
void slsi_conn_log2us_disconnect(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *bssid, int reason);
void slsi_conn_log2us_eapol_gtk(struct slsi_dev *sdev, struct net_device *dev, int eapol_msg_type);
void slsi_conn_log2us_eapol_ptk(struct slsi_dev *sdev, struct net_device *dev, int eapol_msg_type);
void slsi_conn_log2us_roam_scan_start(struct slsi_dev *sdev, struct net_device *dev, int reason,
				      int roam_rssi_val, short chan_utilisation,
				      int rssi_thresh, u64 timestamp);
void slsi_conn_log2us_roam_result(struct slsi_dev *sdev, struct net_device *dev,
				  char *bssid, u64 timestamp, bool roam_candidate);
void slsi_conn_log2us_eap(struct slsi_dev *sdev, struct net_device *dev, u8 *eap_type);
void slsi_conn_log2us_dhcp(struct slsi_dev *sdev, struct net_device *dev, char *str);
void slsi_conn_log2us_dhcp_tx(struct slsi_dev *sdev, struct net_device *dev,
			      char *str, char *tx_status);
void slsi_conn_log2us_eap_with_len(struct slsi_dev *sdev, struct net_device *dev,
				   u8 *eap_type, int eap_length);
void slsi_conn_log2us_auth_req(struct slsi_dev *sdev, struct net_device *dev,
			       const unsigned char *bssid,
			       int auth_algo, int sae_type, int sn,
			       int status, u32 tx_status, int is_roaming);

void slsi_conn_log2us_auth_resp(struct slsi_dev *sdev, struct net_device *dev,
				const unsigned char *bssid,
				int auth_algo,
				int sae_type,
				int sn, int status, int is_roaming);
void slsi_conn_log2us_assoc_req(struct slsi_dev *sdev, struct net_device *dev,
				const unsigned char *bssid,
				int sn, int tx_status, int mgmt_frame_subtype);
void slsi_conn_log2us_assoc_resp(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *bssid,
				 int sn, int status, int mgmt_frame_subtype);

void slsi_conn_log2us_deauth(struct slsi_dev *sdev, struct net_device *dev, char *str_type,
			     const unsigned char *bssid, int sn, int status);

void slsi_conn_log2us_disassoc(struct slsi_dev *sdev, struct net_device *dev, char *str_type,
			       const unsigned char *bssid, int sn, int status);
void slsi_conn_log2us_roam_scan_done(struct slsi_dev *sdev, struct net_device *dev, u64 timestamp);
void slsi_conn_log2us_roam_scan_result(struct slsi_dev *sdev, struct net_device *dev, bool curr,
				       char *bssid, int freq,
				       int rssi, short cu,
				       int score, int tp_score, bool eligible);
void slsi_conn_log2us_btm_query(struct slsi_dev *sdev, struct net_device *dev,
				int dialog, int reason);
void slsi_conn_log2us_btm_req(struct slsi_dev *sdev, struct net_device *dev,
			      int dialog, int btm_mode,
			      int disassoc_timer,
			      int validity_time, int candidate_count);
void slsi_conn_log2us_btm_resp(struct slsi_dev *sdev, struct net_device *dev,
			       int dialog,
			       int btm_mode, int delay, char *bssid);
u8 *get_eap_type_from_val(int val, u8 *str);
void slsi_conn_log2us_eapol_tx(struct slsi_dev *sdev, struct net_device *dev, u32 status_code);
void slsi_conn_log2us_eapol_ptk_tx(struct slsi_dev *sdev, u32 status_code);
void slsi_conn_log2us_eapol_gtk_tx(struct slsi_dev *sdev, u32 status_code);
void slsi_conn_log2us_eap_tx(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
			     int eap_length, int eap_type,
			     char *str_type);
void slsi_conn_log2us_btm_cand(struct slsi_dev *sdev, struct net_device *dev,
			       char *bssid, int prefer);
void slsi_eapol_eap_handle_tx_status(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
				     u16 a, u16 b);
void slsi_conn_log2us_roam_scan_save(struct slsi_dev *sdev, struct net_device *dev, int scan_type,
				     int freq_count, int *freq_list);
void slsi_conn_log2us_nr_frame_req(struct slsi_dev *sdev, struct net_device *dev);

void slsi_conn_log2us_nr_frame_resp(struct slsi_dev *sdev, struct net_device *dev, char *string);
void slsi_conn_log2us_beacon_report_request(struct slsi_dev *sdev, struct net_device *dev,
					    int dialog_token, int operating_class, char *string,
					    int measure_duration, char *measure_mode,
					    char *mac_addr, char *ssid);
void slsi_conn_log2us_beacon_report_response(struct slsi_dev *sdev, struct net_device *dev, int dialog_token,
					     int ap_count, int reason_code);
void slsi_conn_log2us_ncho_mode(struct slsi_dev *sdev, struct net_device *dev, int enable);
#endif
