/****************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/types.h>
#include "debug.h"
#include "dev.h"
#include "sap.h"
#include "sap_ma.h"
#include "hip.h"
#include "ba.h"
#include "mgt.h"
#include "nl80211_vendor.h"
#include "hip4_sampler.h"
#include "traffic_monitor.h"

#ifdef CONFIG_SCSC_WLAN_ANDROID
#include "scsc_wifilogger_rings.h"
#endif

#ifdef CONFIG_SCSC_WLAN_TX_API
#include "tx_api.h"
#endif

#define SUPPORTED_OLD_VERSION   0

static int sap_ma_version_supported(u16 version);
static int sap_ma_rx_handler(struct slsi_dev *sdev, struct sk_buff *skb);
#ifdef CONFIG_SCSC_WLAN_TX_API
static int sap_ma_txdone(struct slsi_dev *sdev, u32 colour, bool more);
#else
static int sap_ma_txdone(struct slsi_dev *sdev,  u8 vif, u8 peer_index, u8 ac);
#endif
static int sap_ma_notifier(struct slsi_dev *sdev, unsigned long event);

static struct sap_api sap_ma = {
	.sap_class = SAP_MA,
	.sap_version_supported = sap_ma_version_supported,
	.sap_handler = sap_ma_rx_handler,
	.sap_versions = { FAPI_DATA_SAP_VERSION, SUPPORTED_OLD_VERSION },
	.sap_txdone = sap_ma_txdone,
	.sap_notifier = sap_ma_notifier,
};

static int sap_ma_notifier(struct slsi_dev *sdev, unsigned long event)
{
	uint vif;

	SLSI_INFO_NODEV("Notifier event received: %lu\n", event);
	if (event >= SCSC_MAX_NOTIFIER)
		return -EIO;

	switch (event) {
	case SCSC_WIFI_STOP:
		SLSI_INFO_NODEV("Stop netdev queues\n");
		rcu_read_lock();
		for (vif = SLSI_NET_INDEX_WLAN;
		     vif <= SLSI_NET_INDEX_P2PX_SWLAN; vif++) {
			struct net_device *ndev =
				slsi_get_netdev_rcu(sdev, vif);
			if (ndev && !netif_queue_stopped(ndev))
				netif_tx_stop_all_queues(ndev);
		}
		rcu_read_unlock();
		break;

	case SCSC_WIFI_FAILURE_RESET:
		SLSI_DBG1_NODEV(SLSI_NETDEV, "Netdevs queues will not be restarted - recovery will take care of it\n");
		break;

	case SCSC_WIFI_SUSPEND:
		break;

	case SCSC_WIFI_RESUME:
		break;
	case SCSC_WIFI_CHIP_READY:
		break;
	case SCSC_WIFI_SUBSYSTEM_RESET:
		break;
	default:
		SLSI_INFO_NODEV("Unknown event code %lu\n", event);
		break;
	}

	return 0;
}

static int sap_ma_version_supported(u16 version)
{
	unsigned int major = SAP_MAJOR(version);
	unsigned int minor = SAP_MINOR(version);
	u8           i = 0;

	SLSI_INFO_NODEV("Reported version: %d.%d\n", major, minor);

	for (i = 0; i < SAP_MAX_VER; i++)
		if (SAP_MAJOR(sap_ma.sap_versions[i]) == major)
			return 0;

	SLSI_ERR_NODEV("Version %d.%d Not supported\n", major, minor);

	return -EINVAL;
}

static bool slsi_rx_check_mc_addr_regd(struct slsi_dev *sdev, struct net_device *dev, struct ethhdr *ehdr)
{
	int i;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	slsi_spinlock_lock(&ndev_vif->sta.regd_mc_addr_lock);

	for (i = 0 ; i < ndev_vif->sta.regd_mc_addr_count; i++)
		if (memcmp(ehdr->h_dest, ndev_vif->sta.regd_mc_addr[i], ETH_ALEN) == 0) {
			SLSI_INFO(sdev, "Wakeup by regd mc addr " MACSTR "\n", MAC2STR(ndev_vif->sta.regd_mc_addr[i]));
			slsi_spinlock_unlock(&ndev_vif->sta.regd_mc_addr_lock);
			return true;
		}

	SLSI_ERR(sdev, "Wakeup by unregistered mc address\n");
	SLSI_INFO(sdev, "Received packet source : " MACSTR ", dest : " MACSTR "\n", MAC2STR(ehdr->h_source), MAC2STR(ehdr->h_dest));
	SLSI_INFO(sdev, "Regd mc addr : \n");
	for (i = 0 ; i < ndev_vif->sta.regd_mc_addr_count; i++)
		SLSI_INFO(sdev, "    " MACSTR "\n", MAC2STR(ndev_vif->sta.regd_mc_addr[i]));

	slsi_spinlock_unlock(&ndev_vif->sta.regd_mc_addr_lock);
	return false;
}

#ifdef CONFIG_SCSC_WLAN_RX_NAPI_GRO
static int slsi_rx_amsdu_deaggregate(struct net_device *dev, struct sk_buff *skb, struct sk_buff_head *msdu_list)
{
	struct slsi_skb_cb *skb_cb = slsi_skb_cb_get(skb);
	unsigned int data_len = 0;
	unsigned int subframe_len = 0;
	unsigned char padding = 0;
	struct sk_buff *subframe = NULL;
	const unsigned char mac_0[ETH_ALEN] = { 0 };
	struct ethhdr *mh = NULL;
	bool discard;

	SLSI_NET_DBG4(dev, SLSI_RX, "A-MSDU received (len:%d)\n", skb->len);
	while (skb->len) {
		/* MSDU format.
		 * MSDU is 4 byte aligned execpt for the last one, i.e., the last frame does not have padding.
		 * Dst: Destination MAC
		 * Src: Source MAC
		 * Len: Data length
		 * SNAP->ETH_TYPE: EtherType
		 * +-----+-----+--------+-----------------------------+----------------------+------+-----+
		 * | Dst | Src |   Len  |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) |   (2)  | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+--------+-----------------------------+----------------------+------+-----+
		 *             ^msdu_len^
		 */
		if (unlikely((ETH_ALEN * 2 + 2) > skb->len)) {
			SLSI_NET_ERR(dev, "invalid SKB length %d < %d\n", skb->len, (ETH_ALEN * 2 + 2));
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}

		data_len = (skb->data[ETH_ALEN * 2] << 8) | skb->data[(ETH_ALEN * 2) + 1];
		if (unlikely(data_len <= LLC_SNAP_HDR_LEN)) {
			SLSI_NET_ERR(dev, "invalid data length %d < %d\n", data_len, LLC_SNAP_HDR_LEN);
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}
		data_len -= LLC_SNAP_HDR_LEN;
		/* check if the length of sub-frame is valid.
		 * <---------------------(ETH_ALEN * 2 + 2 + 8 + data_len)---------------------->
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 * | Dst | Src | Len |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) | (2) | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 */
		if (unlikely(ETH_ALEN * 2 + 2 + LLC_SNAP_HDR_LEN + data_len > skb->len)) {
			SLSI_NET_ERR(dev, "invalid MSDU data length %d, SKB length = %d\n", data_len, skb->len);
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}
		/* We convert MSDU to Ethernet II format.
		 * <--------subframe_len--------->
		 * <---(struct ethhdr)--->
		 * +-----+-----+----------+------+
		 * | Dst | Src | ETH_TYPE | Data |
		 * | (6) | (6) |   (2)    |      |
		 * +-----+-----+----------+------+
		 */
		subframe_len = sizeof(struct ethhdr) + data_len;

		discard = (skb_cb->discard & 0x1 ? true : false);
		skb_cb->discard = skb_cb->discard >> 1;
		/* handle last subframe */
		if (skb->len == subframe_len + LLC_SNAP_HDR_LEN) {
			if (discard) {
				kfree_skb(skb);
				return 0;
			}

			SLSI_ETHER_COPY(&skb->data[14], &skb->data[6]);
			SLSI_ETHER_COPY(&skb->data[8], &skb->data[0]);

			skb_pull(skb, LLC_SNAP_HDR_LEN);

			skb_set_mac_header(skb, 0);
			mh = eth_hdr(skb);
			if (SLSI_ETHER_EQUAL(mh->h_dest, mac_0)) {
				SLSI_NET_DBG3(dev, SLSI_RX, "msdu subframe filtered out: MAC destination address %pM\n", mh->h_dest);
				kfree_skb(skb);
			} else {
				__skb_queue_tail(msdu_list, skb);
			}

			return 0;
		}

		padding = (4 - (subframe_len % 4)) & 0x3;
		if (discard) {
			skb_pull(skb, LLC_SNAP_HDR_LEN + subframe_len + padding);
			continue;
		}
		/* Allocate subframe skbuff */
		subframe = alloc_skb(subframe_len, GFP_ATOMIC);
		if (unlikely(!subframe)) {
			SLSI_NET_ERR(dev, "failed to alloc the SKB for A-MSDU subframe\n");
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -ENOMEM;
		}
		/* Copy Destination and Source MAC
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 * | Dst | Src | Len |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) | (2) | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 * ^
		 * skb->data
		 * +-----+-----+
		 * | Dst | Src |
		 * | (6) | (6) |
		 * +-----+-----+
		 * ^
		 * skb_put(subframe, ETH_ALEN * 2)
		 */
		skb_put_data(subframe, skb->data, ETH_ALEN * 2);
		skb_pull(skb, ETH_ALEN * 2);
		/* Skip MSDU length field, LLC header and SNAP OUI field
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 * | Dst | Src | Len |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) | (2) | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 *                                                          ^
		 *             <-------------------(8)--------------------->skb->data
		 */
		skb_pull(skb, 8);
		/* Copy ETH_TYPE.
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 * | Dst | Src | Len |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) | (2) | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 *                                                          ^
		 *                                                          skb->data
		 * +-----+-----+----------+
		 * | Dst | Src | ETH_TYPE |
		 * | (6) | (6) |   (2)    |
		 * +-----+-----+----------+
		 *             ^
		 *             skb_put(subframe, 2)
		 */
		skb_put_data(subframe, skb->data, 2);
		skb_pull(skb, 2);
		/* Copy data.
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 * | Dst | Src | Len |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) | (2) | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 *                                                                        ^
		 *                                                                        skb->data
		 * +-----+-----+----------+------+
		 * | Dst | Src | ETH_TYPE | Data |
		 * | (6) | (6) |   (2)    |      |
		 * +-----+-----+----------+------+
		 *                        ^
		 *                        skb_put(subframe, data_len)
		 */
		skb_put_data(subframe, skb->data, data_len);
		skb_pull(skb, data_len);

		/* Find padding size and skip the padding.
		 * +----------+-----+-------
		 * | (n)-MSDU | Pad | (n+1)-MSDU
		 * |          |     |
		 * +----------+-----+-------
		 *                  ^
		 *                  skb->data
		 */
		if (unlikely(!skb_pull(skb, padding))) {
			kfree_skb(subframe);
			SLSI_NET_ERR(dev, "Invalid A-MSDU Padding\n");
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}
		/* Before preparing the skb, filter out if the Destination Address of the Ethernet frame
		 * or A-MSDU subframe is set to an invalid value, i.e. all zeros
		 */
		skb_set_mac_header(subframe, 0);
		mh = eth_hdr(subframe);
		if (unlikely(SLSI_ETHER_EQUAL(mh->h_dest, mac_0))) {
			SLSI_NET_DBG3(dev, SLSI_RX, "msdu subframe filtered out: MAC destination address %pM\n", mh->h_dest);
			kfree_skb(subframe);
			continue;
		}
		__skb_queue_tail(msdu_list, subframe);
	}
	return 0;
}
#else
static int slsi_rx_amsdu_deaggregate(struct net_device *dev, struct sk_buff *skb, struct sk_buff_head *msdu_list)
{
	struct slsi_skb_cb *skb_cb = slsi_skb_cb_get(skb);
	unsigned int data_len = 0;
	unsigned int subframe_len = 0;
	unsigned char padding = 0;
	struct sk_buff *subframe = NULL;
	const unsigned char mac_0[ETH_ALEN] = { 0 };
	struct ethhdr *mh = NULL;
	bool discard;
	bool is_last = false;

	SLSI_NET_DBG4(dev, SLSI_RX, "A-MSDU received (len:%d)\n", skb->len);
	while (skb->len) {
		/* MSDU format.
		 * MSDU is 4 byte aligned execpt for the last one, i.e., the last frame does not have padding.
		 * Dst: Destination MAC
		 * Src: Source MAC
		 * Len: Data length
		 * SNAP->ETH_TYPE: EtherType
		 * +-----+-----+--------+-----------------------------+----------------------+------+-----+
		 * | Dst | Src |   Len  |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) |   (2)  | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+--------+-----------------------------+----------------------+------+-----+
		 *             ^msdu_len^
		 */
		if (unlikely((ETH_ALEN * 2 + 2) > skb->len)) {
			SLSI_NET_ERR(dev, "invalid SKB length %d < %d\n", skb->len, (ETH_ALEN * 2 + 2));
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}

		data_len = (skb->data[ETH_ALEN * 2] << 8) | skb->data[(ETH_ALEN * 2) + 1];
		if (unlikely(data_len <= LLC_SNAP_HDR_LEN)) {
			SLSI_NET_ERR(dev, "invalid data length %d < %d\n", data_len, LLC_SNAP_HDR_LEN);
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}
		data_len -= LLC_SNAP_HDR_LEN;
		/* check if the length of sub-frame is valid.
		 * <---------------------(ETH_ALEN * 2 + 2 + 8 + data_len)---------------------->
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 * | Dst | Src | Len |             LLC             |         SNAP         | Data | Pad |
		 * | (6) | (6) | (2) | DSAP(1) | SSAP(1) | Ctrl(1) | OUI(3) | ETH_TYPE(2) |      |     |
		 * +-----+-----+-----+-----------------------------+----------------------+------+-----+
		 */
		if (unlikely(ETH_ALEN * 2 + 2 + LLC_SNAP_HDR_LEN + data_len > skb->len)) {
			SLSI_NET_ERR(dev, "invalid MSDU data length %d, SKB length = %d\n", data_len, skb->len);
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}

		/* We convert MSDU to Ethernet II format.
		 * <--------subframe_len--------->
		 * <---(struct ethhdr)--->
		 * +-----+-----+----------+------+
		 * | Dst | Src | ETH_TYPE | Data |
		 * | (6) | (6) |   (2)    |      |
		 * +-----+-----+----------+------+
		 */
		subframe_len = sizeof(struct ethhdr) + data_len;
		discard = (skb_cb->discard & 0x1 ? true : false);
		skb_cb->discard = skb_cb->discard >> 1;

		/* handle last subframe */
		if (skb->len == subframe_len + LLC_SNAP_HDR_LEN) {
			if (discard) {
				kfree_skb(skb);
				return 0;
			}

			subframe = skb;
			padding = 0;
			is_last = true;
		} else {
			padding = (4 - (subframe_len % 4)) & 0x3;
			if (discard) {
				skb_pull(skb, LLC_SNAP_HDR_LEN + subframe_len + padding);
				continue;
			}

			subframe = skb_clone(skb, GFP_ATOMIC);
			if (unlikely(!subframe)) {
				SLSI_NET_ERR(dev, "failed to clone SKB for A-MSDU subframe\n");
				__skb_queue_purge(msdu_list);
				kfree_skb(skb);
				return -ENOMEM;
			}
		}

		/* set tail pointer of cloned skb
		 * +-----+-----+-----+-----+-------------+-----+------+-----+------+-----+
		 * | Dst | Src | Len | LLC | SNAP | Data | Pad |  subframe  |  subframe  |
		 * | (6) | (6) | (2) | (3) |  (5) |      |     |            |            |
		 * +-----+-----+-----+-----+-------------+-----+------+-----+------+-----+
		 *                                       ^
		 *                                       skb_trim(subframe, subframe_len + LLC_SNAP_HDR_LEN)
		 */
		skb_trim(subframe, subframe_len + LLC_SNAP_HDR_LEN);

		/* Cut LLC/OUI and Set Destination and Source MAC, set subframe->dat and skb->data
		 * +-----+-----+-----+-----+-------------+-----+
		 * | Dst | Src | Len | LLC | SNAP | Data | Pad |
		 * | (6) | (6) | (2) | (3) |  (5) |      |     |
		 * +-----+-----+-----+-----+-------------+-----+
		 *         +-----+-----+----------+------+-----+
		 *         | Dst | Src | ETH_TYPE | Data | Pad |
		 *         | (6) | (6) |   (2)    |      |     |
		 *         +-----+-----+----------+------+-----+
		 *         ^
		 *         skb_pull(subframe, LLC_SNAP_HDR_LEN);
		 *         skb_pull(skb, LLC_SNAP_HDR_LEN);
		 */
		SLSI_ETHER_COPY(&subframe->data[14], &subframe->data[6]);
		SLSI_ETHER_COPY(&subframe->data[8], &subframe->data[0]);
		skb_pull(subframe, LLC_SNAP_HDR_LEN);

		/* Before preparing the skb, filter out if the Destination Address of the Ethernet frame
		 * or A-MSDU subframe is set to an invalid value, i.e. all zeros
		 */
		skb_set_mac_header(subframe, 0);
		mh = eth_hdr(subframe);
		if (unlikely(SLSI_ETHER_EQUAL(mh->h_dest, mac_0))) {
			SLSI_NET_DBG3(dev, SLSI_RX, "msdu subframe filtered out: MAC destination address %pM\n", mh->h_dest);
			skb_pull(skb, LLC_SNAP_HDR_LEN + subframe_len + padding);
			kfree_skb(subframe);
			if (!is_last)
				continue;
		} else {
			__skb_queue_tail(msdu_list, subframe);
		}

		if (is_last)
			return 0;

		/* Set skb->data to next subframe
		 * +-----+-----+-----+-----+-------------+-----+------+-----+------+-----+
		 * | Dst | Src | Len | LLC | SNAP | Data | Pad |  subframe  |  subframe  |
		 * | (6) | (6) | (2) | (3) |  (5) |      |     |            |            |
		 * +-----+-----+-----+-----+-------------+-----+------+-----+------+-----+
		 *                                             ^
		 *                                             skb_pull(skb, subframe_len)
		 */
		if (unlikely(!skb_pull(skb, LLC_SNAP_HDR_LEN + subframe_len + padding))) {
			SLSI_NET_ERR(dev, "Invalid A-MSDU Padding\n");
			__skb_queue_purge(msdu_list);
			kfree_skb(skb);
			return -EINVAL;
		}
	}
	return 0;
}
#endif

static void slsi_rx_check_opt_out_packet(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct ethhdr *ehdr = (struct ethhdr *)(skb->data);

	ndev_vif->is_opt_out_packet = false;

	if (is_broadcast_ether_addr(ehdr->h_dest)) {
		ndev_vif->is_opt_out_packet = true;
		SLSI_NET_ERR(dev, "Wakeup by Broadcast packet\n");
	} else if (is_multicast_ether_addr(ehdr->h_dest)) {
		ndev_vif->is_opt_out_packet = !slsi_rx_check_mc_addr_regd(sdev, dev, ehdr);
	} else if (be16_to_cpu(ehdr->h_proto) == ETH_P_ARP) {
		u8 *frame;
		u16 arp_opcode;

		frame = skb->data + sizeof(struct ethhdr);
		arp_opcode = frame[SLSI_ARP_OPCODE_OFFSET] << 8 | frame[SLSI_ARP_OPCODE_OFFSET + 1];

		/* not enhanced : only request opcode opt out
		 * enhanced : if (enhanced_pkt_filter_enabled) - ALL pkt opt out
		 *                else - only request opcode opt out
		 */
		if (arp_opcode == SLSI_ARP_REQUEST_OPCODE)
			ndev_vif->is_opt_out_packet = true;
#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
		else
			ndev_vif->is_opt_out_packet = sdev->enhanced_pkt_filter_enabled;
#endif
		SLSI_NET_ERR(dev, "Wakeup by ARP packet, arp_opcode : %d\n", arp_opcode);
#ifdef CONFIG_SCSC_WLAN_ENHANCED_PKT_FILTER
	} else if (sdev->enhanced_pkt_filter_enabled) {
		if (is_unicast_ether_addr(ehdr->h_dest)) {
			/* IPv4 & TCP / IPv6 & TCP : opt in */
			if (be16_to_cpu(ehdr->h_proto) == ETH_P_IP) {
				struct iphdr *ip = (struct iphdr *)(skb->data + sizeof(struct ethhdr));

				ndev_vif->is_opt_out_packet = !(ip->protocol == IPPROTO_TCP);
				SLSI_NET_INFO(dev, "Wakeup by IPV4 , ip->protocol = %d\n", ip->protocol);
			} else if (be16_to_cpu(ehdr->h_proto) == ETH_P_IPV6) {
				struct ipv6hdr *ip = (struct ipv6hdr *)(skb->data + sizeof(struct ethhdr));

				ndev_vif->is_opt_out_packet = !(ip->nexthdr == IPPROTO_TCP);
				SLSI_NET_INFO(dev, "Wakeup by IPV6 , ip->nexthdr = %d\n", ip->nexthdr);
			} else {
				ndev_vif->is_opt_out_packet = true;
				SLSI_NET_ERR(dev, "Wakeup by h_proto : 0x%.4X\n", be16_to_cpu(ehdr->h_proto));
			}
		} else {
			SLSI_NET_ERR(dev, "Wakeup by Not Unicast - the packet must be checking in Multicast condition\n");
		}
#endif
	} else {
		SLSI_NET_INFO(dev, "Wakeup by Opt in packet\n");
	}
}

static struct sk_buff *slsi_parse_and_pull_ma_unitdata_ind_signal(struct slsi_dev *sdev, struct sk_buff *skb)
{
	struct slsi_skb_cb *skb_cb;
	bool is_ciphered, is_wakeup;
	u8 discard;

	/* 0.save info in fapi signal and remove fapi signal
	 * KeyRSC	B3	RX: is The received frame ciphered? The first 48bit contains the KeyRSC
	 * Discard	B8-15	RX only: When set to 1, which a-msdu subframe to host shall discared. (B8 first subframe B15 8th subframe)
	 */
	is_ciphered = (fapi_get_u16(skb, u.ma_unitdata_ind.configuration_option) & FAPI_OPTION_KEYRSC) >> 3;
	discard = (fapi_get_u16(skb, u.ma_unitdata_ind.configuration_option) & FAPI_OPTION_DISCARD) >> 8;
	is_wakeup = ((struct slsi_skb_cb *)skb->cb)->wakeup;
#ifdef CONFIG_SCSC_SMAPPER
	/* Get if the skb is in the SMAPPER entry */
	if (slsi_skb_cb_get(skb)->smapper_linked) {
		skb = slsi_hip_get_skb_from_smapper(sdev, skb);
		if (!skb) {
			SLSI_WARN(sdev, "SKB from SMAPPER is NULL\n");
			return NULL;
		}
	} else {
		skb_pull(skb, fapi_get_siglen(skb));
	}
#else
	skb_pull(skb, fapi_get_siglen(skb));
#endif
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->is_amsdu = false;
	skb_cb->is_ciphered = is_ciphered;
	skb_cb->discard = discard;
	skb_cb->wakeup = is_wakeup;

	return skb;
}

static int slsi_get_msdu_frame(struct net_device *dev, struct sk_buff *skb)
{
	struct slsi_skb_cb   *skb_cb = (struct slsi_skb_cb *)skb->cb;
	struct ieee80211_hdr *hdr;
	int offset = 0;
	u8 *qc = NULL;

	hdr = (struct ieee80211_hdr *)skb->data;

	if (!ieee80211_is_data(hdr->frame_control)) {
		SLSI_NET_DBG3(dev, SLSI_RX, "Ignore this packet. It is not data\n");
		return -EINVAL;
	}

	if (ieee80211_has_fromds(hdr->frame_control) || ieee80211_has_tods(hdr->frame_control))
		skb_cb->is_tdls = false;
	else
		skb_cb->is_tdls = true;

	/* 1.get seq control */
	skb_cb->seq_num = (le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_SEQ) >> 4;

	/* 2.calc offset & del 80211_hdr */
	if (ieee80211_has_a4(hdr->frame_control)) {
		offset += sizeof(struct ieee80211_hdr);
		SLSI_NET_DBG4(dev, SLSI_RX, "80211 header has 4addr offset:%d\n", offset);
	} else {
		offset += sizeof(struct ieee80211_hdr_3addr);
	}

	if (ieee80211_is_data_qos(hdr->frame_control)) {
		qc = ieee80211_get_qos_ctl(hdr);
		offset += IEEE80211_QOS_CTL_LEN;
		SLSI_NET_DBG4(dev, SLSI_RX, "80211 header has QoS_CTL offset:%d\n", offset);
		/* save priority from qos_ctl */
		skb_cb->tid = qc[0] & IEEE80211_QOS_CTL_TID_MASK;
	} else {
		/* save priority for non-QoS tid */
		skb_cb->tid = FAPI_PRIORITY_CONTENTION;
	}

	if (ieee80211_has_order(hdr->frame_control)) {
		offset += IEEE80211_HT_CTL_LEN;
		SLSI_NET_DBG4(dev, SLSI_RX, "80211 header has HT_CTL offset:%d\n", offset);
	}

	/* 3.store key-RSC if FAPI_OPTION_KEYRSC is set */
	if (skb_cb->is_ciphered) {
		memcpy(skb_cb->keyrsc, skb->data + offset, SLSI_EAPOL_KEY_RSC_LENGTH);
		offset += SLSI_EAPOL_KEY_RSC_LENGTH;
		SLSI_NET_DBG4(dev, SLSI_RX, "key-RSC is contained offset:%d\n", offset);
	}

	if (qc && (*qc & IEEE80211_QOS_CTL_A_MSDU_PRESENT)) {
		/* 4.A-MSDU payload  just pull 80211_hdr */
		skb_cb->is_amsdu = true;
		skb_pull(skb, offset);
		SLSI_NET_DBG4(dev, SLSI_RX, "Packet is A-MSDU offset:%d\n", offset);
	} else {
		/* 4.MSDU remove llc and add eth_hdr */
		SLSI_ETHER_COPY(&skb->data[offset], ieee80211_get_SA(hdr));
		/**
		 * skb->data[offset - ETH_ALEN] and ieee80211_get_DA(hdr)
		 * can have memory overlapping which leads to data corruption.
		 * Especially when addr3 is used for DA.
		 * We use memmove which considers memory overlapping case.
		 * https://man7.org/linux/man-pages/man3/memmove.3.html
		 */
		memmove(&skb->data[offset - ETH_ALEN], ieee80211_get_DA(hdr), ETH_ALEN);
		skb_pull(skb, offset - ETH_ALEN);
		SLSI_NET_DBG4(dev, SLSI_RX, "Packet is MSDU offset:%d\n", offset);
	}

	return 0;
}

void slsi_rx_data_deliver_skb(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, bool ctx_napi)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff_head msdu_list;
	struct slsi_skb_cb *skb_cb = (struct slsi_skb_cb *)skb->cb;
	struct slsi_peer *peer = NULL;
	struct ethhdr *eth_hdr;
	bool is_amsdu = skb_cb->is_amsdu;
	u8 trafic_q = slsi_frame_priority_to_ac_queue(skb_cb->tid);

	__skb_queue_head_init(&msdu_list);

	eth_hdr = (struct ethhdr *)skb->data;
	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr->h_source);
	if (!peer) {
		SLSI_NET_WARN(dev, "Packet dropped (no peer records)\n");
		kfree_skb(skb);
		return;
	}

	/* A-MSDU deaggregation */
	if (is_amsdu) {
		if (slsi_rx_amsdu_deaggregate(dev, skb, &msdu_list)) {
			SLSI_NET_WARN(dev, "Packet dropped (A-MSDU error)\n");
			ndev_vif->stats.rx_dropped++;
			if (peer)
				peer->sinfo.rx_dropped_misc++;
			return;
		}
	} else {
		if (skb_cb->discard) {
			SLSI_NET_WARN(dev, "Packet dropped (discard flag set)\n");
			/* is a duplicate frame from control plane, that should be
			 * discarded.
			 * not needed to update drop stats here
			 */
			consume_skb(skb);
			return;
		}
		__skb_queue_tail(&msdu_list, skb);
	}
	/* WARNING: skb may be NULL here and should not be used after this */
	while (!skb_queue_empty(&msdu_list)) {
		struct sk_buff *rx_skb;
#if defined(CONFIG_SCSC_WLAN_RX_NAPI) && defined(CONFIG_SCSC_WLAN_RX_NAPI_GRO)
		u16 drop_proto;
#endif

		rx_skb = __skb_dequeue(&msdu_list);

		/* In STA mode, if Wakeup by Opt out packet, it looks it was
		 * NOT install packet filter when enter Suspend mode. So it
		 * should be install Packet filter and LCD state again.
		 */
		if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
			struct ethhdr *ehdr = (struct ethhdr *)(rx_skb->data);

			if (unlikely(slsi_skb_cb_get(rx_skb)->wakeup)) {
				slsi_rx_check_opt_out_packet(sdev, dev, rx_skb);
				slsi_wake_lock(&sdev->wlan_wl);
				schedule_work(&ndev_vif->update_pkt_filter_work);
			}

			/* In STA mode, the AP relays back our multicast
			 * traffic. Receiving these frames and passing it up
			 * confuses some protocols and applications, notably
			 * IPv6 Duplicate Address Detection.
			 *
			 * So these frames are dropped instead of passing it
			 * further. No need to update the drop statistics as
			 * these frames are locally generated and should not be
			 * accounted in reception.
			 */
			if (is_multicast_ether_addr(ehdr->h_dest) && !compare_ether_addr(ehdr->h_source, dev->dev_addr)) {
				SLSI_NET_DBG2(dev, SLSI_RX, "drop locally generated multicast frame relayed back by AP\n");
				consume_skb(rx_skb);
				continue;
			}
		}

		/* Intra BSS */
		if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && ndev_vif->peer_sta_records) {
			struct slsi_peer *peer = NULL;
			struct ethhdr *ehdr = (struct ethhdr *)(rx_skb->data);

			if (is_multicast_ether_addr(ehdr->h_dest)) {
				/* For the case of uing NAPI, we need to use GFP_ATOMIC */
				struct sk_buff *rebroadcast_skb = skb_copy(rx_skb, GFP_ATOMIC);
				if (!rebroadcast_skb) {
					SLSI_WARN(sdev, "Intra BSS: failed to alloc new SKB for broadcast\n");
				} else {
					SLSI_DBG3(sdev, SLSI_RX, "Intra BSS: multicast %pM\n", ehdr->h_dest);
					rebroadcast_skb->dev = dev;
					rebroadcast_skb->protocol = cpu_to_be16(ETH_P_802_3);
					skb_reset_network_header(rebroadcast_skb);
					skb_reset_mac_header(rebroadcast_skb);
					dev_queue_xmit(rebroadcast_skb);
				}
			} else {
				peer = slsi_get_peer_from_mac(sdev, dev, ehdr->h_dest);
				if (peer && peer->authorized) {
					SLSI_DBG3(sdev, SLSI_RX, "Intra BSS: unicast %pM\n", ehdr->h_dest);
					rx_skb->dev = dev;
					rx_skb->protocol = cpu_to_be16(ETH_P_802_3);
					skb_reset_network_header(rx_skb);
					skb_reset_mac_header(rx_skb);
					dev_queue_xmit(rx_skb);
					continue;
				}
			}
		}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
		/* NAN: multicast receive */
		if (ndev_vif->ifnum >= SLSI_NAN_DATA_IFINDEX_START) {
			struct net_device *other_dev;
			struct netdev_vif *other_ndev_vif;
			struct ethhdr *ehdr = (struct ethhdr *)(rx_skb->data);
			u8 i, j;

			if (is_multicast_ether_addr(ehdr->h_dest)) {
				/* In case of NAN, the multicast packet is received on a NDL VIF.
				 * NDL VIF is between two peers.
				 * But two peers can have multiple NDP connections.
				 * Multiple NDP connections with same peer needs multiple netdevices.
				 *
				 * So, loop through each net device and find if there is a match in
				 * source address, and if so, duplicate the multicast packet, and send
				 * them up on each netdev that matches.
				 */
				SLSI_NET_DBG2(dev, SLSI_RX, "NAN: multicast (source address: %pM)\n", ehdr->h_source);
				for ((i = ndev_vif->ifnum + 1); i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
					other_dev = (struct net_device *) sdev->netdev[i];
					if (!other_dev)
						continue;
					other_ndev_vif = (struct netdev_vif *)netdev_priv(other_dev);
					if (!other_ndev_vif)
						continue;
					for (j = 0; j < SLSI_PEER_INDEX_MAX; j++) {
						if (other_ndev_vif->peer_sta_record[j] &&
							other_ndev_vif->peer_sta_record[j]->valid &&
						    ether_addr_equal(other_ndev_vif->peer_sta_record[j]->address, ehdr->h_source)) {
							/* For the case of uing NAPI, we need to use GFP_ATOMIC */
							struct sk_buff *duplicate_skb = skb_copy(rx_skb, GFP_ATOMIC);
							SLSI_NET_DBG2(other_dev, SLSI_RX, "NAN: source address match %pM\n", other_ndev_vif->peer_sta_record[j]->address);
							if (!duplicate_skb) {
								SLSI_NET_WARN(other_dev, "NAN: multicast: failed to alloc new SKB\n");
								continue;
							}

							other_ndev_vif->peer_sta_record[j]->sinfo.rx_bytes += duplicate_skb->len;
							other_ndev_vif->stats.rx_packets++;
							other_ndev_vif->stats.rx_bytes += duplicate_skb->len;
							other_ndev_vif->rx_packets[trafic_q]++;

							duplicate_skb->dev = other_dev;
							duplicate_skb->ip_summed = CHECKSUM_NONE;
							duplicate_skb->protocol = eth_type_trans(duplicate_skb, other_dev);

							SLSI_NET_DBG4(other_dev, SLSI_RX, "pass %u bytes to local stack\n", duplicate_skb->len);
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
							netif_receive_skb(duplicate_skb);
#else
							netif_rx_ni(duplicate_skb);
#endif
						}
					}
				}
			}
		}
#endif
		if (peer) {
			peer->sinfo.rx_bytes += rx_skb->len;
		}
		ndev_vif->stats.rx_packets++;
		ndev_vif->stats.rx_bytes += rx_skb->len;
		ndev_vif->rx_packets[trafic_q]++;

#ifdef CONFIG_SCSC_WLAN_STA_ENHANCED_ARP_DETECT
	if (!ndev_vif->enhanced_arp_stats.is_duplicate_addr_detected) {
		u8 *frame = rx_skb->data + 12; /* frame points to packet type */
		u16 packet_type = frame[0] << 8 | frame[1];

		if (packet_type == ETH_P_ARP) {
			frame = frame + 2; /* ARP packet */
			/*match source IP address in ARP with the DUT Ip address*/
			if ((frame[SLSI_ARP_SRC_IP_ADDR_OFFSET] == (ndev_vif->ipaddress & 255)) &&
			    (frame[SLSI_ARP_SRC_IP_ADDR_OFFSET + 1] == ((ndev_vif->ipaddress >>  8U) & 255)) &&
			    (frame[SLSI_ARP_SRC_IP_ADDR_OFFSET + 2] == ((ndev_vif->ipaddress >> 16U) & 255)) &&
			    (frame[SLSI_ARP_SRC_IP_ADDR_OFFSET + 3] == ((ndev_vif->ipaddress >> 24U) & 255)) &&
			    !SLSI_IS_GRATUITOUS_ARP(frame) &&
			    !SLSI_ETHER_EQUAL(dev->dev_addr, frame + 8)) /*if src MAC = DUT MAC */
				ndev_vif->enhanced_arp_stats.is_duplicate_addr_detected = 1;
		}
	}

	if (ndev_vif->enhanced_arp_detect_enabled && (ndev_vif->vif_type == FAPI_VIFTYPE_STATION)) {
		u8 *frame = rx_skb->data + 12; /* frame points to packet type */
		u16 packet_type = frame[0] << 8 | frame[1];
		u16 arp_opcode;

		if (packet_type == ETH_P_ARP) {
			frame = frame + 2; /* ARP packet */
			arp_opcode = frame[SLSI_ARP_OPCODE_OFFSET] << 8 | frame[SLSI_ARP_OPCODE_OFFSET + 1];
			/* check if sender ip = gateway ip and it is an ARP response */
			if ((arp_opcode == SLSI_ARP_REPLY_OPCODE) &&
			    !SLSI_IS_GRATUITOUS_ARP(frame) &&
			    !memcmp(&frame[SLSI_ARP_SRC_IP_ADDR_OFFSET], &ndev_vif->target_ip_addr, 4)) {
				ndev_vif->enhanced_arp_stats.arp_rsp_count_to_netdev++;
				ndev_vif->enhanced_arp_stats.arp_rsp_rx_count_by_upper_mac++;
			}
		}
	}
#endif

		rx_skb->dev = dev;
		rx_skb->ip_summed = CHECKSUM_NONE;
		rx_skb->protocol = eth_type_trans(rx_skb, dev);

		SCSC_HIP4_SAMPLER_TCP_DECODE(sdev, dev, rx_skb->data, true);
		slsi_traffic_mon_event_rx(sdev, dev, rx_skb);

		SLSI_NET_DBG4(dev, SLSI_RX, "pass %u bytes to local stack\n", rx_skb->len);
		slsi_skb_cb_init(rx_skb);
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
#ifdef CONFIG_SCSC_WLAN_RX_NAPI_GRO
#ifdef CONFIG_SCSC_WLAN_NW_PKT_DROP
		/* We directly call protocol handler to ensure that
		 * ARP response is properly processed regardless of filters.
		 */
		if (rx_skb->protocol == htons(ETH_P_ARP)) {
			bypass_backlog(sdev, dev, rx_skb);
			return;
		}
#endif
		drop_proto = rx_skb->protocol;
		if (ctx_napi) {
			if (GRO_DROP == napi_gro_receive(&sdev->hip4_inst.hip_priv->napi, rx_skb))
				SLSI_NET_WARN(dev, "Packet is dropped. Protocol=%hx\n", ntohs(drop_proto));
		} else {
			if (NET_RX_DROP == netif_receive_skb(rx_skb))
				SLSI_NET_WARN(dev, "Packet is dropped. Protocol=%hx\n", ntohs(drop_proto));
		}
#else /* #ifdef CONFIG_SCSC_WLAN_RX_NAPI_GRO */
		netif_receive_skb(rx_skb);
#endif
#else /* #ifdef CONFIG_SCSC_WLAN_RX_NAPI */
		netif_rx_ni(rx_skb);
#endif
		slsi_wake_lock_timeout(&sdev->wlan_wl_ma, msecs_to_jiffies(SLSI_RX_WAKELOCK_TIME));
	}
}

static void slsi_rx_data_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_skb_cb *skb_cb;
	struct slsi_peer *peer = NULL;
	struct ethhdr *eth_hdr;

	skb = slsi_parse_and_pull_ma_unitdata_ind_signal(sdev, skb);
	if (!skb)
		return;

	skb_cb = (struct slsi_skb_cb *)skb->cb;
	/* pass the data up if the VIF type is Monitor */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_MONITOR) {
		skb_reset_mac_header(skb);
		skb->dev = dev;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb->pkt_type = PACKET_OTHERHOST;
		netif_rx_ni(skb);
		return;
	}

	if (slsi_get_msdu_frame(dev, skb)) {
		WARN_ON(1);
		kfree_skb(skb);
		return;
	}
	eth_hdr = (struct ethhdr *)skb->data;

	/* Populate wake reason stats here */
	if (unlikely(slsi_skb_cb_get(skb)->wakeup)) {
		skb->mark = SLSI_WAKEUP_PKT_MARK;
		slsi_rx_update_wake_stats(sdev, eth_hdr, skb->len);
	}

	SLSI_NET_DBG4(dev, SLSI_RX, "ma_unitdata_ind(vif:%d, dest:%pM, src:%pM, tid:%d, seq:%d, cipher:%d, discard:%X, is_amsdu:%d, is_tdls:%d)\n",
		      ndev_vif->ifnum,
		      eth_hdr->h_dest,
		      eth_hdr->h_source,
		      skb_cb->tid,
		      skb_cb->seq_num,
		      skb_cb->is_ciphered,
		      skb_cb->discard,
		      skb_cb->is_amsdu,
		      skb_cb->is_tdls);

	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr->h_source);
	if (!peer) {
		SLSI_NET_WARN(dev, "Packet dropped (no peer records)\n");
		/* Race in Data plane (Shows up in fw test mode) */
		kfree_skb(skb);
		return;
	}

	/* discard data frames if received before key negotiations are completed */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_AP && peer->connected_state != SLSI_STA_CONN_STATE_CONNECTED) {
		SLSI_NET_WARN(dev, "Packet dropped (peer connection not complete (state:%u))\n", peer->connected_state);
		kfree_skb(skb);
		return;
	}

	/* skip BA reorder if the destination address is Multicast */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION && (is_multicast_ether_addr(eth_hdr->h_dest))) {
		/* Skip BA reorder and pass the frames Up */
		SLSI_NET_DBG2(dev, SLSI_RX, "Multicast/Broadcast packet received in STA mode(seq: %d) skip BA\n", skb_cb->seq_num);
		slsi_rx_data_deliver_skb(sdev, dev, skb, false);
		return;
	}

	/* When TDLS connection has just been closed a few last frame may still arrive from the closed connection.
	 * This frames must not be injected in to the block session with the AP as the sequence numbers are different
	 * that will confuse the BA process. Therefore we have to skip BA for those frames.
	 */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION && peer->aid < SLSI_TDLS_PEER_INDEX_MIN && skb_cb->is_tdls) {
		if (printk_ratelimit())
			SLSI_NET_WARN(dev, "Packet received from TDLS but no TDLS exists (seq: %x) Skip BA\n", skb_cb->seq_num);

		/* Skip BA reorder and pass the frames Up */
		slsi_rx_data_deliver_skb(sdev, dev, skb, false);
		return;
	}

	/* TDLS is enabled for the PEER but still packet is received through the AP. Process this packet with the AP PEER */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION && peer->aid >= SLSI_TDLS_PEER_INDEX_MIN && !skb_cb->is_tdls) {
		SLSI_NET_DBG2(dev, SLSI_TDLS, "Packet received from TDLS peer through the AP(seq: %x)\n", skb_cb->seq_num);
		peer = slsi_get_peer_from_qs(sdev, dev, SLSI_STA_PEER_QUEUESET);
		if (!peer) {
			SLSI_NET_WARN(dev, "Packet dropped (AP peer not found)\n");
			kfree_skb(skb);
			return;
		}
	}

	/* If frame belongs to a negotiated BA, BA will consume the frame */
	if (slsi_ba_check(peer, skb_cb->tid))
		if (!slsi_ba_process_frame(dev, peer, skb, skb_cb->seq_num, skb_cb->tid))
			return;

	/* store the PN even when BA agreement is not setup yet */
	slsi_ba_replay_store_pn(dev, peer, skb);

	/* Pass to next receive process */
	slsi_rx_data_deliver_skb(sdev, dev, skb, false);
}

#ifdef CONFIG_SCSC_WLAN_RX_NAPI
static int slsi_rx_napi_process(struct slsi_dev *sdev, struct sk_buff *skb)
{
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	u16 vif;

	vif = fapi_get_vif(skb);

	rcu_read_lock();
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	if ((vif >= SLSI_NAN_DATA_IFINDEX_START) && fapi_get_sigid(skb) == MA_UNITDATA_IND) {
		dev = slsi_nan_get_netdev_rcu(sdev, skb);
	} else {
		dev = slsi_get_netdev_rcu(sdev, vif);
	}
#else
	dev = slsi_get_netdev_rcu(sdev, vif);
#endif
	if (!dev) {
		SLSI_ERR(sdev, "netdev(%d) No longer exists\n", vif);
		rcu_read_unlock();
		return -EINVAL;
	}
	rcu_read_unlock();

	ndev_vif = netdev_priv(dev);

	slsi_debug_frame(sdev, dev, skb, "RX");
	switch (fapi_get_u16(skb, id)) {
	case MA_UNITDATA_IND:
		slsi_rx_data_ind(sdev, dev, skb);

		/* SKBs in a BA session are not passed yet */
		slsi_spinlock_lock(&ndev_vif->ba_lock);
		if (atomic_read(&ndev_vif->ba_flush)) {
			atomic_set(&ndev_vif->ba_flush, 0);
			slsi_ba_process_complete(dev, true);
		}
		slsi_spinlock_unlock(&ndev_vif->ba_lock);
		break;
	default:
		SLSI_DBG1(sdev, SLSI_RX, "Unexpected Data: 0x%.4x\n", fapi_get_sigid(skb));
		kfree_skb(skb);
		break;
	}
	return 0;
}
#else
void slsi_rx_netdev_data_work(struct work_struct *work)
{
	struct slsi_skb_work *w = container_of(work, struct slsi_skb_work, work);
	struct slsi_dev *sdev = w->sdev;
	struct net_device *dev = w->dev;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *skb;

	if (WARN_ON(!dev))
		return;

	slsi_wake_lock(&sdev->wlan_wl);

	while (1) {
		SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
		if (!ndev_vif->activated) {
			skb_queue_purge(&w->queue);
			SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
			break;
		}

		slsi_spinlock_lock(&ndev_vif->ba_lock);
		if (atomic_read(&ndev_vif->ba_flush)) {
			atomic_set(&ndev_vif->ba_flush, 0);
			slsi_ba_process_complete(dev, false);
		}
		slsi_spinlock_unlock(&ndev_vif->ba_lock);

		skb = slsi_skb_work_dequeue(w);
		if (!skb) {
			SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
			break;
		}

		slsi_debug_frame(sdev, dev, skb, "RX");
		switch (fapi_get_u16(skb, id)) {
		case MA_UNITDATA_IND:
#ifdef CONFIG_SCSC_SMAPPER
			if (slsi_skb_cb_get(skb)->smapper_linked) {
				u8 *frame = (u8 *)slsi_hip_get_skb_data_from_smapper(sdev, skb);

				if (frame)
					SCSC_HIP4_SAMPLER_TCP_DECODE(sdev, dev, frame, false);
			} else {
				SCSC_HIP4_SAMPLER_TCP_DECODE(sdev, dev, skb->data + fapi_get_siglen(skb), false);
			}
#else
			SCSC_HIP4_SAMPLER_TCP_DECODE(sdev, dev, skb->data + fapi_get_siglen(skb), false);
#endif
			slsi_rx_data_ind(sdev, dev, skb);
			break;
		default:
			SLSI_DBG1(sdev, SLSI_RX, "Unexpected Data: 0x%.4x\n", fapi_get_sigid(skb));
			kfree_skb(skb);
			break;
		}
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	}
	slsi_wake_unlock(&sdev->wlan_wl);
	/* reprocess hip_wq for skipped data BH */
	slsi_hip_reprocess_skipped_data_bh(sdev);

}

static int slsi_rx_queue_data(struct slsi_dev *sdev, struct sk_buff *skb)
{
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	int vif;

	vif = fapi_get_vif(skb);

	rcu_read_lock();
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	if (vif >= SLSI_NAN_DATA_IFINDEX_START && fapi_get_sigid(skb) == MA_UNITDATA_IND) {
		dev = slsi_nan_get_netdev_rcu(sdev, skb);
	} else {
		dev = slsi_get_netdev_rcu(sdev, vif);
	}
#else
	dev = slsi_get_netdev_rcu(sdev, vif);
#endif

	if (!dev) {
		SLSI_ERR(sdev, "netdev(%d) No longer exists\n", vif);
		rcu_read_unlock();
		goto err;
	}
	ndev_vif = netdev_priv(dev);

	slsi_skb_work_enqueue(&ndev_vif->rx_data, skb);
	rcu_read_unlock();
	return 0;
err:
	return -EINVAL;
}
#endif

static int sap_ma_rx_handler(struct slsi_dev *sdev, struct sk_buff *skb)
{
#ifdef CONFIG_SCSC_SMAPPER
	u16 sig_len;
	u32 err;
#endif

	switch (fapi_get_sigid(skb)) {
	case MA_UNITDATA_IND:
#ifdef CONFIG_SCSC_SMAPPER
		/* Check SMAPPER to nullify entry*/
		if (slsi_skb_cb_get(skb)->smapper_linked) {
			sig_len = fapi_get_siglen(skb);
			skb_pull(skb, sig_len);
			err = slsi_hip_consume_smapper_entry(sdev, skb);
			skb_push(skb, sig_len);
			if (err)
				return err;
		}
#endif
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
		return slsi_rx_napi_process(sdev, skb);
#else
		return slsi_rx_queue_data(sdev, skb);
#endif
	default:
		break;
	}
	SLSI_ERR_NODEV("Shouldn't be getting here!\n");
	return -EINVAL;
}

/* Adjust the scod value and flow control appropriately. */
#ifdef CONFIG_SCSC_WLAN_TX_API
static int sap_ma_txdone(struct slsi_dev *sdev, u32 colour, bool more)
{
	return slsi_tx_done(sdev, colour, more);
}
#else
static int sap_ma_txdone(struct slsi_dev *sdev,  u8 vif, u8 peer_index, u8 ac)
{
	struct net_device *dev;
	struct slsi_peer *peer;

	rcu_read_lock();
	dev = slsi_get_netdev_rcu(sdev, vif);

	if (!dev) {
		SLSI_ERR(sdev, "netdev(%d) No longer exists\n", vif);
		rcu_read_unlock();
		return -EINVAL;
	}
	rcu_read_unlock();

	if (peer_index <= SLSI_PEER_INDEX_MAX) {
		/* peer_index = 0 for Multicast queues */
		if (peer_index == 0) {
			struct netdev_vif *ndev_vif = netdev_priv(dev);

			return scsc_wifi_fcq_receive_data(dev, &ndev_vif->ap.group_data_qs, ac, sdev, vif, peer_index);
		}
		peer = slsi_get_peer_from_qs(sdev, dev, MAP_AID_TO_QS(peer_index));
		if (peer)
			return scsc_wifi_fcq_receive_data(dev, &peer->data_qs, ac, sdev, vif, peer_index);

		SLSI_DBG1(sdev, SLSI_RX, "peer record NOT found for vif=%d peer_index=%d\n", vif, peer_index);
		/* We need to handle this case as special. Peer disappeared bug hip4
		 * is sending back the colours to free.
		 */
		return scsc_wifi_fcq_receive_data_no_peer(dev, ac, sdev, vif, peer_index);
	}
	SLSI_ERR(sdev, "illegal peer_index vif=%d peer_index=%d\n", vif, peer_index);
	return -EINVAL;
}
#endif

int sap_ma_init(void)
{
	SLSI_INFO_NODEV("Registering SAP\n");
	slsi_hip_sap_register(&sap_ma);
	return 0;
}

int sap_ma_deinit(void)
{
	SLSI_INFO_NODEV("Unregistering SAP\n");
	slsi_hip_sap_unregister(&sap_ma);
	return 0;
}
