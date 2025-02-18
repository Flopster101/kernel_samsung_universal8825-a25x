/******************************************************************************
 *
 * Copyright (c) 2012 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include "dev.h"
#include "debug.h"
#include "mgt.h"
#include "mlme.h"
#include "netif.h"
#include "log_clients.h"
#include "hip4_sampler.h"
#include "traffic_monitor.h"
#include "scsc_wifilogger_ring_pktfate_api.h"
#include "log2us.h"

static bool msdu_enable = true;
module_param(msdu_enable, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(msdu_enable, "MSDU frame format, Y: enable (default), N: disable");

#ifdef CONFIG_SCSC_WLAN_TX_API
__always_inline bool is_msdu_enable(void)
{
	return msdu_enable;
}
#endif

#ifdef CONFIG_SCSC_WLAN_ANDROID
#include "scsc_wifilogger_rings.h"
#endif

/**
 * Needed to get HIP4_DAT)SLOTS...should be part
 * of initialization and callbacks registering
 */
#include "hip4.h"

#ifdef CONFIG_SCSC_WLAN_TX_API
#include "tx_api.h"
#endif

#include <linux/spinlock.h>


static int slsi_get_miclen(struct net_device *dev, u32 akm_suite)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	switch (akm_suite) {
	case SLSI_KEY_MGMT_802_1X_SUITE_B_192:
	case SLSI_KEY_MGMT_FT_802_1X_SHA384:
		return 24;
	case SLSI_KEY_MGMT_OWE:
		if (ndev_vif->sta.owe_group_during_connection == 20)
			return 24;
		else if (ndev_vif->sta.owe_group_during_connection == 21)
			return 32;
		else
			return 16;
	default:
		return 16;
	}
}

int slsi_get_dwell_time_for_wps(struct slsi_dev *sdev, struct netdev_vif *ndev_vif, u8 *eapol, u16 eap_length)
{
	/**
	 * Note that Message should not be M8.This check is to identify only
	 * WSC_START message or M1-M7
	 * Return 100ms If opcode type WSC msg and Msg Type M1-M7 or if
	 * opcode is WSC start.
	 */
	if (eapol[SLSI_EAP_CODE_POS] == SLSI_EAP_PACKET_REQUEST ||
	    eapol[SLSI_EAP_CODE_POS] == SLSI_EAP_PACKET_RESPONSE) {
		if (eapol[SLSI_EAP_TYPE_POS] == SLSI_EAP_TYPE_EXPANDED && eap_length >= SLSI_EAP_OPCODE_POS - 3 &&
		    ((eapol[SLSI_EAP_OPCODE_POS] == SLSI_EAP_OPCODE_WSC_MSG && eap_length >= SLSI_EAP_MSGTYPE_POS - 3 &&
		    eapol[SLSI_EAP_MSGTYPE_POS] != SLSI_EAP_MSGTYPE_M8) ||
		    eapol[SLSI_EAP_OPCODE_POS] == SLSI_EAP_OPCODE_WSC_START))
			return SLSI_EAP_WPS_DWELL_TIME;
		/**
		 * This is to check if a frame is EAP request identity and on
		 * P2P vif.If yes then set dwell time to 100ms
		 */
		if (SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif) &&
		    eapol[SLSI_EAP_CODE_POS] == SLSI_EAP_PACKET_REQUEST &&
		    eapol[SLSI_EAP_TYPE_POS] == SLSI_EAP_TYPE_IDENTITY)
			return SLSI_EAP_WPS_DWELL_TIME;
	}
	return 0;
}

#ifndef CONFIG_SCSC_WLAN_TX_API
static int slsi_tx_eapol(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer;
	u8 *eapol = NULL;
	u16 msg_type = 0;
	u16 proto = ntohs(skb->protocol);
	int ret = 0;
	u32 dwell_time = sdev->fw_dwell_time;
	u64 tx_bytes_tmp = 0;
	u16 eap_length = 0;
	u16 frame_len = skb->len - sizeof(struct ethhdr);
	struct slsi_wpa_eapol_key *key = NULL;
	u8 *keydatalen_pos, *mic;
	u16 keydatalen = 0;

	slsi_spinlock_lock(&ndev_vif->peer_lock);
	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr(skb)->h_dest);
	if (!peer) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "no peer record for " MACSTR ", drop EAP frame\n", MAC2STR(eth_hdr(skb)->h_dest));
		return -EINVAL;
	}

	switch (proto) {
	case ETH_P_PAE:
		 /**
		  * Detect if this is an EAPOL key frame. If so detect if
		  * it is an EAPOL-Key M4 packet
		  * In M4 packet,
		  * - Key type bit set in key info (pairwise=1, Group=0)
		  * - ACK bit will not be set
		  * - Secure bit will be set in key type RSN (WPA2/WPA3
		  *   Personal/WPA3 Enterprise)
		  * - Key Data length check for Zero for wpa or RSN or
		  *   Key Data length will be 16 when miclen is Zero and Encr data is set in RSN Key type
		  */
		if (frame_len < 4)
			break;

		eapol = skb->data + sizeof(struct ethhdr);
		key = (struct slsi_wpa_eapol_key *)(eapol);
		mic = (u8 *)(key + 1);
		keydatalen_pos  = (mic +  slsi_get_miclen(dev, ndev_vif->sta.crypto.akm_suites[0]));
		keydatalen = ((keydatalen_pos[0] << 8) | keydatalen_pos[1]);

		if (key->type == SLSI_IEEE8021X_TYPE_EAPOL_KEY && frame_len >= 99) {
			msg_type = FAPI_MESSAGETYPE_EAPOL_KEY_M123;

			if ((key->key_info[0] & SLSI_EAPOL_KEY_INFO_REQUEST_BIT_IN_HIGHER_BYTE) ||
			    !(key->key_info[1] & SLSI_EAPOL_KEY_INFO_KEY_TYPE_BIT_IN_LOWER_BYTE)) {
				msg_type = FAPI_MESSAGETYPE_EAPOL_KEY_M123;
			} else {
				if (!(key->key_info[1] & SLSI_EAPOL_KEY_INFO_ACK_BIT_IN_LOWER_BYTE)) {
					if ((key->key_info[0] & SLSI_EAPOL_KEY_INFO_SECURE_BIT_IN_HIGHER_BYTE) ||
					    keydatalen == 0 ||
					    ((key->key_info[0] & SLSI_EAPOL_KEY_INFO_MIC_BIT_IN_HIGHER_BYTE) &&
					    (key->key_info[0] & SLSI_EAPOL_KEY_INFO_ENCR_DATA_BIT_IN_HIGHER_BYTE) &&
					    keydatalen == SLSI_AES_BLOCK_SIZE)) {
						msg_type = FAPI_MESSAGETYPE_EAPOL_KEY_M4;
						dwell_time = 0;
					}
				}
			}

		} else {
			msg_type = FAPI_MESSAGETYPE_EAP_MESSAGE;

			dwell_time = 0;
			if (frame_len >= 9 && eapol[SLSI_EAPOL_IEEE8021X_TYPE_POS] ==
			    SLSI_IEEE8021X_TYPE_EAP_PACKET) {
				eap_length = (skb->len - sizeof(struct ethhdr)) - 4;
				if (eapol[SLSI_EAP_CODE_POS] == SLSI_EAP_PACKET_REQUEST) {
					SLSI_INFO(sdev, "Send EAP-Request (%d)\n", eap_length);
				} else if (eapol[SLSI_EAP_CODE_POS] == SLSI_EAP_PACKET_RESPONSE) {
					SLSI_INFO(sdev, "Send EAP-Response (%d)\n", eap_length);
					if (ndev_vif->iftype == NL80211_IFTYPE_STATION)
						sdev->conn_log2us_ctx.eap_resp_len = eap_length;
				} else if (eapol[SLSI_EAP_CODE_POS] == SLSI_EAP_PACKET_SUCCESS) {
					SLSI_INFO(sdev, "Send EAP-Success (%d)\n", eap_length);
				} else if (eapol[SLSI_EAP_CODE_POS] == SLSI_EAP_PACKET_FAILURE) {
					SLSI_INFO(sdev, "Send EAP-Failure (%d)\n", eap_length);
				}
				/**
				 * Need to set dwell time for wps exchange and
				 * EAP identity frame for P2P
				 */
				dwell_time = slsi_get_dwell_time_for_wps(sdev, ndev_vif, eapol, eap_length);
			}
		}
	break;
	case ETH_P_WAI:
		SLSI_NET_DBG1(dev, SLSI_MLME, "WAI protocol frame\n");
		msg_type = FAPI_MESSAGETYPE_WAI_MESSAGE;
		/* subtype 9 refers to unicast negotiation response */
		if (skb->data[17] != 9)
			dwell_time = 0;
	break;
	default:
		SLSI_NET_WARN(dev, "protocol NOT supported\n");
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		return -EOPNOTSUPP;
	}

	/* EAPOL/WAI frames are send via the MLME */
	tx_bytes_tmp = skb->len; /*len copy to avoid null pointer of skb*/
	ret = slsi_mlme_send_frame_data(sdev, dev, skb, msg_type, 0, dwell_time, 0);
	if (!ret)
		peer->sinfo.tx_bytes += tx_bytes_tmp; //skb->len;

	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	return ret;
}

static int slsi_tx_arp(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif	*ndev_vif = netdev_priv(dev);
	struct slsi_peer	*peer;
	u8	*frame;
	u16	arp_opcode;
	int	ret = 0;
	u32	dwell_time = 0;

	slsi_spinlock_lock(&ndev_vif->peer_lock);

	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr(skb)->h_dest);
	if (!peer && !is_broadcast_ether_addr(eth_hdr(skb)->h_dest)) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "no peer record for " MACSTR ", drop ARP frame\n", MAC2STR(eth_hdr(skb)->h_dest));
		return -EINVAL;
	}

	if (peer) {
		spin_lock_bh(&peer->data_qs.cp_lock);
		/* Controlled port is not yet open; so can't send ARP frame */
		if (peer->data_qs.controlled_port_state == SCSC_WIFI_FCQ_8021x_STATE_BLOCKED) {
			SLSI_DBG1_NODEV(SLSI_MLME, " ARP 8021x_STATE_BLOCKED\n");
			spin_unlock_bh(&peer->data_qs.cp_lock);
			slsi_spinlock_unlock(&ndev_vif->peer_lock);
			return -EPERM;
		}
		spin_unlock_bh(&peer->data_qs.cp_lock);
	}

	frame = skb->data + sizeof(struct ethhdr);
	arp_opcode = frame[SLSI_ARP_OPCODE_OFFSET] << 8 | frame[SLSI_ARP_OPCODE_OFFSET + 1];
	if (arp_opcode == SLSI_ARP_REQUEST_OPCODE &&
	    !SLSI_IS_GRATUITOUS_ARP(frame)) {
#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
		if (ndev_vif->enhanced_arp_detect_enabled &&
		    !memcmp(&frame[SLSI_ARP_DEST_IP_ADDR_OFFSET], &ndev_vif->target_ip_addr, 4)) {
			ndev_vif->enhanced_arp_stats.arp_req_count_from_netdev++;
		}
#endif
		dwell_time = sdev->fw_dwell_time;
	}
	ret = slsi_mlme_send_frame_data(sdev, dev, skb, FAPI_MESSAGETYPE_ARP, 0, dwell_time, 0);
	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	return ret;
}

void slsi_dump_msgtype(struct slsi_dev *sdev, u32 dhcp_message_type)
{
	if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_DISCOVER)
		SLSI_INFO(sdev, "Send DHCP [DISCOVER]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_OFFER)
		SLSI_INFO(sdev, "Send DHCP [OFFER]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_REQUEST)
		SLSI_INFO(sdev, "Send DHCP [REQUEST]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_DECLINE)
		SLSI_INFO(sdev, "Send DHCP [DECLINE]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_ACK)
		SLSI_INFO(sdev, "Send DHCP [ACK]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_NAK)
		SLSI_INFO(sdev, "Send DHCP [NAK]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_RELEASE)
		SLSI_INFO(sdev, "Send DHCP [RELEASE]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_INFORM)
		SLSI_INFO(sdev, "Send DHCP [INFORM]\n");
	else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_FORCERENEW)
		SLSI_INFO(sdev, "Send DHCP [FORCERENEW]\n");
	else
		SLSI_INFO(sdev, "Send DHCP [INVALID]\n");
}

static int slsi_tx_dhcp(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif	*ndev_vif = netdev_priv(dev);
	struct slsi_peer	*peer;
	int	                ret = 0;
	u32	                dwell_time = 0;
	u32	                dhcp_message_type = SLSI_DHCP_MESSAGE_TYPE_INVALID;
	u8	                *str_type = NULL;
	u8	                *str_tx_status;

	slsi_spinlock_lock(&ndev_vif->peer_lock);
	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr(skb)->h_dest);
	if (!peer && !is_broadcast_ether_addr(eth_hdr(skb)->h_dest)) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "no peer record for " MACSTR ", drop DHCP frame\n", MAC2STR(eth_hdr(skb)->h_dest));
		return -EINVAL;
	}

	if (peer) {
		spin_lock_bh(&peer->data_qs.cp_lock);
		/* Controlled port is not yet open; so can't send DHCP frame */
		if (peer->data_qs.controlled_port_state == SCSC_WIFI_FCQ_8021x_STATE_BLOCKED) {
			SLSI_DBG1_NODEV(SLSI_MLME, "dhcp 8021x_STATE_BLOCKED\n");
			spin_unlock_bh(&peer->data_qs.cp_lock);
			slsi_spinlock_unlock(&ndev_vif->peer_lock);
			return -EPERM;
		}
		spin_unlock_bh(&peer->data_qs.cp_lock);
	}

	if (skb->len >= 285 && slsi_is_dhcp_packet(skb->data) != SLSI_TX_IS_NOT_DHCP) {
		/* opcode 1 refers to DHCP discover/request */
		if (skb->data[42] == 1)
			dwell_time = sdev->fw_dwell_time;
		dhcp_message_type = skb->data[284];
		slsi_dump_msgtype(sdev, dhcp_message_type);
		ret = slsi_mlme_send_frame_data(sdev, dev, skb, FAPI_MESSAGETYPE_DHCP, 0, dwell_time, 0);

		if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_DISCOVER)
			str_type = "DISCOVER";
		else if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_REQUEST)
			str_type = "REQUEST";

		if (ret)
			str_tx_status = "TX_FAIL";
		else
			str_tx_status = "ACK";
		if (dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_DISCOVER || dhcp_message_type == SLSI_DHCP_MESSAGE_TYPE_REQUEST)
			slsi_conn_log2us_dhcp_tx(sdev, dev, str_type, str_tx_status);

		SLSI_INFO(sdev, "ret = %d\n", ret);
	} else {
		/* IP frame can have only DHCP packet in SLSI_NETIF_Q_PRIORITY */
		SLSI_NET_ERR(dev, "Bad IP frame in SLSI_NETIF_Q_PRIORITY\n");
	}
	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	return ret;
}
#endif
uint slsi_sg_host_align_mask; /* TODO -- this needs to be resolved! */

/**
 * This function deals with TX of data frames.
 * On success, skbs are properly FREED; on error skb is NO MORE freed.
 *
 * NOTE THAT currently ONLY the following set of err-codes will trigger
 * a REQUEUE and RETRY by upper layers in Kernel NetStack:
 *
 *  -ENOSPC
 */
int slsi_tx_data(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif   *ndev_vif = netdev_priv(dev);
#ifndef CONFIG_SCSC_WLAN_TX_API
	struct slsi_skb_cb  *cb;
	struct slsi_peer    *peer;
	u16                 len = skb->len;
	int                 ret = 0;
	u8                  vif_index = 0;
	u8                  peer_index = 0;
	enum slsi_traffic_q tq;
#endif

	if (slsi_is_test_mode_enabled()) {
		/* This signals is in XML file because parts of the Firmware
		 * need the symbols defined by them
		 * but this is actually not handled in wlanlite firmware.
		 */
		SLSI_NET_WARN(dev, "WlanLite: NOT supported\n");
		return -EOPNOTSUPP;
	}

	if (!ndev_vif->activated) {
		SLSI_NET_WARN(dev, "vif NOT activated\n");
		return -EINVAL;
	}

	if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && !ndev_vif->peer_sta_records) {
		SLSI_NET_DBG3(dev, SLSI_TX,
			      "AP with no STAs associated, drop Tx frame\n");
		return -EINVAL;
	}
#ifdef CONFIG_SCSC_WLAN_TX_API
	if (slsi_tx_get_packet_type(skb) == SLSI_CTRL_PKT)
		return slsi_tx_transmit_ctrl(sdev, dev, skb);
#else
	/* check if it is an high important frame? At the moment EAPOL, DHCP
	 * and ARP are treated as high important frame and are sent over
	 * MLME for applying special rules in transmission.
	 */
	if (skb->queue_mapping == SLSI_NETIF_Q_PRIORITY) {
		int proto = be16_to_cpu(eth_hdr(skb)->h_proto);

		switch (proto) {
		default:
			/* Only EAP packets and IP frames with DHCP are stored in SLSI_NETIF_Q_PRIORITY */
			SLSI_NET_ERR(dev,
				     "Bad h_proto=0x%x SLSI_NETIF_Q_PRIORITY\n", proto);
			return -EINVAL;
		case ETH_P_PAE:
		case ETH_P_WAI:
			SLSI_NET_DBG2(dev, SLSI_MLME,
				      "tx EAP pkt from SLSI_NETIF_Q_PRIORITY\n");
			return slsi_tx_eapol(sdev, dev, skb);
		case ETH_P_IP:
			SLSI_NET_DBG2(dev, SLSI_MLME,
				      "tx DHCP pkt from SLSI_NETIF_Q_PRIORITY\n");
			return slsi_tx_dhcp(sdev, dev, skb);
		}
	}

	if (skb->queue_mapping == SLSI_NETIF_Q_ARP) {
		int proto = be16_to_cpu(eth_hdr(skb)->h_proto);

		switch (proto) {
		case ETH_P_ARP:
			SLSI_NET_DBG2(dev, SLSI_MLME,
				      "tx ARP pkt from SLSI_NETIF_Q_ARP\n");
			return slsi_tx_arp(sdev, dev, skb);
		default:
			/* Only ARP packets are stored in SLSI_NETIF_Q_ARP */
			SLSI_NET_ERR(dev,
				     "Bad h_proto=0x%x SLSI_NETIF_Q_ARP\n", proto);
			return -EINVAL;
		}
	}
#endif

	/* Align mac_header with skb->data */
	if (skb_headroom(skb) != skb->mac_header)
		skb_pull(skb, skb->mac_header - skb_headroom(skb));

#ifdef CONFIG_SCSC_WLAN_TX_API
	return slsi_tx_transmit_data(sdev, dev, skb);
#else

	if (msdu_enable)
		ethr_ii_to_subframe_msdu(skb);

	len = skb->len;

	(void)skb_push(skb, fapi_sig_size(ma_unitdata_req));
	tq = slsi_frame_priority_to_ac_queue(skb->priority);
	vif_index = ndev_vif->ifnum;
	peer_index = MAP_QS_TO_AID(slsi_netif_get_qs_from_queue(skb->queue_mapping, tq));
	fapi_set_u16(skb, id,           MA_UNITDATA_REQ);
	fapi_set_u16(skb, receiver_pid, 0);
	fapi_set_u16(skb, sender_pid,   SLSI_TX_PROCESS_ID_MIN);
	fapi_set_u32(skb, fw_reference, 0);
	fapi_set_u16(skb, u.ma_unitdata_req.vif, vif_index);
	fapi_set_u16(skb, u.ma_unitdata_req.configuration_option, FAPI_OPTION_INLINE);
	SLSI_NET_DBG_HEX(dev, SLSI_TX, skb->data, skb->len < 128 ? skb->len : 128, "\n");

	cb = slsi_skb_cb_init(skb);
	cb->sig_length = fapi_sig_size(ma_unitdata_req);
	cb->data_length = skb->len;

	/* ACCESS POINT MODE */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		struct ethhdr *ehdr = eth_hdr(skb);

		if (is_multicast_ether_addr(ehdr->h_dest)) {
			fapi_set_u16(skb, u.ma_unitdata_req.flow_id, FAPI_PRIORITY_CONTENTION >> 8);
			fapi_set_memcpy(skb, u.ma_unitdata_req.address, ehdr->h_source);
			ret = scsc_wifi_fcq_transmit_data(dev,
							  &ndev_vif->ap.group_data_qs,
							  slsi_frame_priority_to_ac_queue(skb->priority),
							  sdev,
							  vif_index,
							  peer_index);
			if (ret < 0) {
				SLSI_NET_WARN(dev, "no fcq for groupcast, drop Tx frame\n");
				return ret;
			}

			ret = scsc_wifi_transmit_frame(&sdev->hip4_inst, skb, false, vif_index, peer_index, slsi_frame_priority_to_ac_queue(skb->priority));
			if (ret == NETDEV_TX_OK) {
				return ret;
			} else if (ret < 0) {
				/* scsc_wifi_transmit_frame failed, decrement BoT counters */
				scsc_wifi_fcq_receive_data(dev,
							   &ndev_vif->ap.group_data_qs,
							   slsi_frame_priority_to_ac_queue(skb->priority),
							   sdev,
							   vif_index,
							   peer_index);
				return ret;
			}
			return -EIO;
		}
	}
	slsi_spinlock_lock(&ndev_vif->peer_lock);

	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr(skb)->h_dest);
	if (!peer) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "no peer record for " MACSTR ", drop Tx frame\n", MAC2STR(eth_hdr(skb)->h_dest));
		return -EINVAL;
	}

	/**
	 * skb->priority will contain the priority obtained from the IP Diff/Serv field.
	 * The skb->priority field is defined in terms of the FAPI_PRIORITY_* definitions.
	 * For QoS enabled associations, this is the tid and is the value required in
	 * the ma_unitdata_req.flow_id field. For non-QoS assocations, the ma_unitdata_req.
	 * flow_id field requires {peer_id | 0x80}.
	 */
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	/* For NAN vif_index is set to ndl_vif */
	if (ndev_vif->ifnum >= SLSI_NAN_DATA_IFINDEX_START) {
		u8 i = 0;

		if (is_multicast_ether_addr(eth_hdr(skb)->h_dest)) {
			SLSI_NET_DBG1(dev, SLSI_TX, "multicast on NAN interface: Source=%pM\n", eth_hdr(skb)->h_source);
			/* TODO: group option will be added to next fapi.xml.
			 * Until then, set configuration_option for NAN multicast with 0x0010
			 */
			fapi_set_u16(skb, u.ma_unitdata_req.configuration_option,
				     FAPI_OPTION_INLINE | 0x0010);
			/**
			 * The driver shall send (duplicate) frames to all VIFs that
			 * have the same source address (SA).  i.e. find other peers on same
			 * netdevice interface and duplicate the packet for each peer
			 */
			for (i = 0; i < SLSI_PEER_INDEX_MAX; i++) {
				if (ndev_vif->peer_sta_record[i] &&
				    ndev_vif->peer_sta_record[i]->valid &&
				    ndev_vif->peer_sta_record[i]->ndl_vif != peer->ndl_vif) {
					/* duplicate the SKB and send to
					 * peer that has a different NDL
					 */
					struct sk_buff *duplicate_skb = skb_copy(skb, GFP_ATOMIC);

					if (!duplicate_skb) {
						SLSI_NET_WARN(dev, "NAN: Tx: multicast: failed to alloc duplicate SKB\n");
						continue;
					}
					fapi_set_u16(duplicate_skb, u.ma_unitdata_req.vif, ndev_vif->peer_sta_record[i]->ndl_vif);

					if (!ndev_vif->peer_sta_record[i]->qos_enabled)
						fapi_set_u16(duplicate_skb, u.ma_unitdata_req.flow_id,
									((FAPI_PRIORITY_CONTENTION >> 8) & 0xff) |
									(ndev_vif->peer_sta_record[i]->flow_id & 0xff00));
					else
						fapi_set_u16(duplicate_skb, u.ma_unitdata_req.flow_id,
									(duplicate_skb->priority & 0xff) |
									(ndev_vif->peer_sta_record[i]->flow_id & 0xff00));

					fapi_set_memcpy(duplicate_skb, u.ma_unitdata_req.address, sdev->nan_cluster_id);
					/* overwrite the multicast destination address with that of Peer */
					ether_addr_copy(eth_hdr(duplicate_skb)->h_dest, ndev_vif->peer_sta_record[i]->address);

					ret = scsc_wifi_fcq_transmit_data(dev, &ndev_vif->peer_sta_record[i]->data_qs,
									  slsi_frame_priority_to_ac_queue(duplicate_skb->priority),
									  sdev,
									  vif_index,
									  ndev_vif->peer_sta_record[i]->aid);
					if (ret < 0) {
						SLSI_NET_WARN(dev, "duplicate SKB: no fcq for " MACSTR ", drop Tx frame\n", MAC2STR(ndev_vif->peer_sta_record[i]->address));
						kfree_skb(duplicate_skb);
						continue;
					}

					ret = scsc_wifi_transmit_frame(&sdev->hip4_inst,
								       duplicate_skb,
								       false,
								       vif_index,
								       ndev_vif->peer_sta_record[i]->aid,
								       slsi_frame_priority_to_ac_queue(duplicate_skb->priority));
					if (ret != NETDEV_TX_OK) {
						/* scsc_wifi_transmit_frame failed, decrement BoT counters */
						scsc_wifi_fcq_receive_data(dev, &ndev_vif->peer_sta_record[i]->data_qs,
									   slsi_frame_priority_to_ac_queue(duplicate_skb->priority),
									   sdev,
									   vif_index,
									   ndev_vif->peer_sta_record[i]->aid);
						kfree_skb(duplicate_skb);
					}
				}
			}
		}
		/* For a NAN VIF, override the local VIF index with NDL VIF for that peer */
		fapi_set_u16(skb, u.ma_unitdata_req.vif, peer->ndl_vif);
		/* overwrite the multicast destination address with that of Peer */
		ether_addr_copy(eth_hdr(skb)->h_dest, peer->address);
	}
#endif

	slsi_debug_frame(sdev, dev, skb, "TX");

	if (!peer->qos_enabled)
		fapi_set_u16(skb, u.ma_unitdata_req.flow_id,
					((FAPI_PRIORITY_CONTENTION >> 8) & 0xff) |
					(peer->flow_id & 0xff00));
	else
		fapi_set_u16(skb, u.ma_unitdata_req.flow_id,
					(skb->priority & 0xff) | (peer->flow_id & 0xff00));

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		if (ndev_vif->sta.tdls_enabled && slsi_is_tdls_peer(dev, peer))
			fapi_set_memcpy(skb, u.ma_unitdata_req.address, ndev_vif->sta.bssid);
		else
			fapi_set_memcpy(skb, u.ma_unitdata_req.address, eth_hdr(skb)->h_dest);
	} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP)
		fapi_set_memcpy(skb, u.ma_unitdata_req.address, eth_hdr(skb)->h_source);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	else if (ndev_vif->ifnum >= SLSI_NAN_DATA_IFINDEX_START)
		fapi_set_memcpy(skb, u.ma_unitdata_req.address, sdev->nan_cluster_id);
#endif

	ret = scsc_wifi_fcq_transmit_data(dev, &peer->data_qs,
					  slsi_frame_priority_to_ac_queue(skb->priority),
					  sdev,
					  vif_index,
					  peer_index);
	if (ret < 0) {
		SLSI_NET_WARN(dev, "no fcq for " MACSTR ", drop Tx frame\n", MAC2STR(eth_hdr(skb)->h_dest));
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		return ret;
	}

	/* SKB is owned by scsc_wifi_transmit_frame() unless the transmission is
	 * unsuccesful.
	 */
	slsi_traffic_mon_event_tx(sdev, dev, skb);
	ret = scsc_wifi_transmit_frame(&sdev->hip4_inst, skb, false, vif_index, peer_index, slsi_frame_priority_to_ac_queue(skb->priority));
	if (ret != NETDEV_TX_OK) {
		/* scsc_wifi_transmit_frame failed, decrement BoT counters */
		scsc_wifi_fcq_receive_data(dev, &peer->data_qs, slsi_frame_priority_to_ac_queue(skb->priority),
					   sdev,
					   vif_index,
					   peer_index);

		if (ret == -ENOSPC) {
			slsi_spinlock_unlock(&ndev_vif->peer_lock);
			return ret;
		}
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		return -EIO;
	}
	/* Frame has been successfully sent, and freed by lower layers */
	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	/* What about the original if we passed in a copy ? */
	peer->sinfo.tx_bytes += len;
	return ret;
#endif
}

int slsi_tx_data_lower(struct slsi_dev *sdev, struct sk_buff *skb)
{
#ifdef CONFIG_SCSC_WLAN_TX_API
	return slsi_tx_transmit_lower(sdev, skb);
#else
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct slsi_peer  *peer;
	u16               vif;
	u8                peer_index = 0;
	u8                *dest = NULL;
	int               ret;

	vif = fapi_get_vif(skb);

	peer_index = fapi_get_u16(skb, u.ma_unitdata_req.flow_id) >> 8;

	/* All MA-UNITDATA.request must be in A-MSDU subframe format.
	 *
	 * The AMSDU frame type is an AMSDU payload ready to be prepended by
	 * an 802.11 frame header by the firmware. The AMSDU subframe header
	 * is identical to an Ethernet header in terms of addressing, so it
	 * is safe to access the destination address through the ethernet
	 * structure.
	 */
	dest = eth_hdr(skb)->h_dest;

	rcu_read_lock();
	dev = slsi_get_netdev_rcu(sdev, vif);
	if (!dev) {
		SLSI_ERR(sdev, "netdev(%d) No longer exists\n", vif);
		rcu_read_unlock();
		return -EINVAL;
	}

	ndev_vif = netdev_priv(dev);
	rcu_read_unlock();

	if (is_multicast_ether_addr(dest) && ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		if (scsc_wifi_fcq_transmit_data(dev, &ndev_vif->ap.group_data_qs,
						slsi_frame_priority_to_ac_queue(skb->priority),
						sdev,
						vif,
						peer_index) < 0) {
			SLSI_NET_DBG3(dev, SLSI_TX, "no fcq for groupcast, dropping TX frame\n");
			return -EINVAL;
		}
		ret = scsc_wifi_transmit_frame(&sdev->hip4_inst, skb, false, vif, peer_index, slsi_frame_priority_to_ac_queue(skb->priority));
		if (ret == NETDEV_TX_OK)
			return ret;
		/**
		 * This should be NEVER RETRIED/REQUEUED and its' handled
		 * by the caller in UDI cdev_write
		 */
		if (ret == -ENOSPC)
			SLSI_NET_DBG1(dev, SLSI_TX, "TX_LOWER...Queue Full... BUT Dropping packet\n");
		else
			SLSI_NET_DBG1(dev, SLSI_TX, "TX_LOWER...Generic Error...Dropping packet\n");
		/* scsc_wifi_transmit_frame failed, decrement BoT counters */
		scsc_wifi_fcq_receive_data(dev, &ndev_vif->ap.group_data_qs,
					   slsi_frame_priority_to_ac_queue(skb->priority),
					   sdev,
					   vif,
					   peer_index);
		return ret;
	}

	slsi_spinlock_lock(&ndev_vif->peer_lock);
	peer = slsi_get_peer_from_mac(sdev, dev, dest);
	if (!peer) {
		SLSI_ERR(sdev, "no peer record for " MACSTR ", dropping TX frame\n",
			 MAC2STR(dest));
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		return -EINVAL;
	}
	slsi_debug_frame(sdev, dev, skb, "TX");

	if ((fapi_get_u16(skb, u.ma_unitdata_req.flow_id) & 0xFF) == (FAPI_PRIORITY_CONTENTION >> 8))
		skb->priority = FAPI_PRIORITY_QOS_UP0;
	else
		skb->priority = fapi_get_u16(skb, u.ma_unitdata_req.flow_id) & 0xFF;

	if (scsc_wifi_fcq_transmit_data(dev, &peer->data_qs,
					slsi_frame_priority_to_ac_queue(skb->priority),
					sdev,
					vif,
					peer_index) < 0) {
		SLSI_NET_DBG3(dev, SLSI_TX, "no fcq for %02x:%02x:%02x:%02x:%02x:%02x, dropping TX frame\n",
			      eth_hdr(skb)->h_dest[0], eth_hdr(skb)->h_dest[1], eth_hdr(skb)->h_dest[2], eth_hdr(skb)->h_dest[3], eth_hdr(skb)->h_dest[4], eth_hdr(skb)->h_dest[5]);
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		return -EINVAL;
	}

	/* SKB is owned by scsc_wifi_transmit_frame() unless the transmission is
	 * unsuccesful.
	 */
	ret = scsc_wifi_transmit_frame(&sdev->hip4_inst, skb, false, vif, peer_index, slsi_frame_priority_to_ac_queue(skb->priority));
	if (ret < 0) {
		SLSI_NET_DBG1(dev, SLSI_TX, "%s (signal: %d)\n", ret == -ENOSPC ? "Queue is full. Flow control" : "Failed to transmit", fapi_get_sigid(skb));
		/* scsc_wifi_transmit_frame failed, decrement BoT counters */
		scsc_wifi_fcq_receive_data(dev, &ndev_vif->ap.group_data_qs,
					   slsi_frame_priority_to_ac_queue(skb->priority),
					   sdev,
					   vif,
					   peer_index);
		if (ret == -ENOSPC)
			SLSI_NET_DBG1(dev, SLSI_TX,
				      "TX_LOWER...Queue Full...BUT Dropping packet\n");
		else
			SLSI_NET_DBG1(dev, SLSI_TX,
				      "TX_LOWER...Generic Error...Dropping packet\n");
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		return ret;
	}

	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	return 0;
#endif
}

/**
 * NOTE:
 * 1. dev can be NULL
 * 2. On error the SKB is NOT freed, NOR retried (ENOSPC dropped).
 * Callers should take care to free the SKB eventually.
 */
int slsi_tx_control(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct slsi_skb_cb *cb;
	int res = 0;
	struct fapi_signal_header *hdr;

	if (WARN_ON(!skb)) {
		res = -EINVAL;
		goto exit;
	}

	/**
	 * Sanity check of the skb - if it's not an MLME, MA, debug or test
	 * signal it will be discarded.
	 * Skip if test mode (wlanlite) is enabled.
	 */
	if (!slsi_is_test_mode_enabled())
		if (!fapi_is_mlme(skb) && !fapi_is_ma(skb) && !fapi_is_debug(skb) && !fapi_is_test(skb)) {
			SLSI_NET_WARN(dev, "Discarding skb because it has type: 0x%04X\n", fapi_get_sigid(skb));
			return -EINVAL;
		}

	cb = slsi_skb_cb_init(skb);
	cb->sig_length = fapi_get_expected_size(skb);
	cb->data_length = skb->len;
	/* F/w will panic if fw_reference is not zero. */
	hdr = (struct fapi_signal_header *)skb->data;
	hdr->fw_reference = 0;

	slsi_debug_frame(sdev, dev, skb, "TX");
	res = scsc_wifi_transmit_frame(&sdev->hip4_inst, skb, true, 0, 0, 0);
	if (res != NETDEV_TX_OK && res != -EINVAL) {
		char reason[80];

		SLSI_NET_ERR(dev, "%s (signal %d)\n", res == -ENOSPC ? "Queue is full. Flow control" : "Failed to transmit", fapi_get_sigid(skb));

		if (!in_interrupt()) {
			snprintf(reason, sizeof(reason), "Failed to transmit signal 0x%04X (err:%d)", fapi_get_sigid(skb), res);
			slsi_sm_service_failed(sdev, reason, false);

			res = -EIO;
		}
	}
exit:
	return res;
}

#ifndef CONFIG_SCSC_WLAN_TX_API
void slsi_tx_pause_queues(struct slsi_dev *sdev)
{
	if (!sdev)
		return;

	scsc_wifi_fcq_pause_queues(sdev);
}

void slsi_tx_unpause_queues(struct slsi_dev *sdev)
{
	if (!sdev)
		return;

	scsc_wifi_fcq_unpause_queues(sdev);
}
#endif
