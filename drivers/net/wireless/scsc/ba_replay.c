/****************************************************************************
 *
 * Copyright (c) 2012 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "debug.h"
#include "dev.h"
#include "ba.h"

static bool ba_replay_check_enable = false;
module_param(ba_replay_check_enable, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ba_replay_check_enable, "Replay check (1: enable (default), 0: disable)");

static uint ba_replay_check_option = 1;
module_param(ba_replay_check_option, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ba_replay_check_option, "Replay check option type (1 (default), 2");

static bool ba_replay_check_option_1(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx, struct slsi_ba_frame_desc *frame_desc);
static bool ba_replay_check_option_2(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx, struct slsi_ba_frame_desc *frame_desc);

struct ba_replay_check_type {
	const u8   option_num;
	bool       (*replay_check_fn)(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx, struct slsi_ba_frame_desc *frame_desc);
};

/* NOTES: Replay detection Options
 *
 * Option 1:
 * 		If an MSDU is a candidate to be passed to upper layers, either as result
 * 		of reordering or as result of reordering timeout, then only pass it if it's
 *		PN is more than the highest PN passed to upper layer so far.
 *
 * Option 2:
 *		If:
 *		An MSDU's SN is "higher" than the "highest" SN (i.e modulo 4096) passed to
 *		upper layers so far, then only pass it if
 * 			(a) there are no lower holes or
 *			(b) it times out
 * 			(c) it's PN is more than the highest PN passed to UL so far
 *
 *		An MSDU’s SN is not “higher” than the “highest” SN (i.e. modulo 4096) passed to
 *		the upper layers so far, then only pass it if
 *			(a) there are no “lower” holes or
 *			(b) it times out or
 *			(c) a “higher” SN has already timed out and been passed to the UL, and if
 *			(d) its PN is less than the highest PN passed to the UL so far
 *
 */

static const struct ba_replay_check_type replay_check_types[] = {
	{ 1,  ba_replay_check_option_1 },
	{ 2,  ba_replay_check_option_2 }
};

void slsi_ba_replay_reset_pn(struct net_device *dev, struct slsi_peer *peer)
{
	u8 i = 0;

	SLSI_NET_DBG4(dev, SLSI_RX_BA, "reset PNs for peer %pM\n", peer->address);
	for (i = 0; i <= FAPI_PRIORITY_QOS_UP7; i++)
		memset(peer->rx_pn[i], 0, SLSI_RX_PN_LEN);
}

void slsi_ba_replay_get_pn(struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb, struct slsi_ba_frame_desc *frame_desc)
{
	struct slsi_skb_cb *skb_cb;

	if (ba_replay_check_enable) {
		skb_cb = (struct slsi_skb_cb *)skb->cb;

		if (skb_cb->is_ciphered) {
			frame_desc->pn[0] = skb_cb->keyrsc[7];
			frame_desc->pn[1] = skb_cb->keyrsc[6];
			frame_desc->pn[2] = skb_cb->keyrsc[5];
			frame_desc->pn[3] = skb_cb->keyrsc[4];
			frame_desc->pn[4] = skb_cb->keyrsc[1];
			frame_desc->pn[5] = skb_cb->keyrsc[0];
			SLSI_NET_DBG4(dev, SLSI_RX_BA, "received: tid=%d sn=%d previous PN %pm received PN %pm\n",
				frame_desc->tid,
				frame_desc->sn,
				peer->rx_pn[frame_desc->tid],
				frame_desc->pn);
		}
	}
}

void slsi_ba_replay_store_pn(struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb)
{
	struct slsi_skb_cb *skb_cb;

	if (ba_replay_check_enable) {
		skb_cb = (struct slsi_skb_cb *)skb->cb;

		if (skb_cb->is_ciphered) {
			if (skb_cb->tid <= FAPI_PRIORITY_QOS_UP7) {
				peer->rx_pn[skb_cb->tid][0] = skb_cb->keyrsc[7];
				peer->rx_pn[skb_cb->tid][1] = skb_cb->keyrsc[6];
				peer->rx_pn[skb_cb->tid][2] = skb_cb->keyrsc[5];
				peer->rx_pn[skb_cb->tid][3] = skb_cb->keyrsc[4];
				peer->rx_pn[skb_cb->tid][4] = skb_cb->keyrsc[1];
				peer->rx_pn[skb_cb->tid][5] = skb_cb->keyrsc[0];

				SLSI_NET_DBG4(dev, SLSI_RX_BA, "store: tid=%d sn=%d received PN %pm\n",
					skb_cb->tid,
					skb_cb->seq_num,
					peer->rx_pn[skb_cb->tid]);
			}
		}
	}
}

bool ba_replay_check_option_1(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx, struct slsi_ba_frame_desc *frame_desc)
{
	struct slsi_peer *peer = ba_session_rx->peer;

	if (frame_desc->flag_old_sn) {
		SLSI_NET_WARN(dev, "old frame, drop: sn=%d, expected_sn=%d\n", frame_desc->sn, ba_session_rx->expected_sn);
		ba_session_rx->ba_drops_old++;
		return true;
	}

	if (memcmp(frame_desc->pn, peer->rx_pn[frame_desc->tid], SLSI_RX_PN_LEN) <= 0) {
		SLSI_NET_WARN(dev, "replay detected: tid=%d sn=%d previous PN %pm received PN %pm\n",
			frame_desc->tid,
			frame_desc->sn,
			peer->rx_pn[frame_desc->tid],
			frame_desc->pn);

		ba_session_rx->ba_drops_replay++;
		return true;
	}

	memcpy(peer->rx_pn[frame_desc->tid], frame_desc->pn, SLSI_RX_PN_LEN);
	return false;
}

bool ba_replay_check_option_2(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx, struct slsi_ba_frame_desc *frame_desc)
{
	struct slsi_peer *peer = ba_session_rx->peer;
	u8 i = 0;

	if (!frame_desc->flag_old_sn) {
		if (memcmp(frame_desc->pn, peer->rx_pn[frame_desc->tid], SLSI_RX_PN_LEN) <= 0) {
			SLSI_NET_WARN(dev, "replay detected: tid=%d sn=%d previous PN %pm received PN %pm\n",
				frame_desc->tid,
				frame_desc->sn,
				peer->rx_pn[frame_desc->tid],
				frame_desc->pn);

			ba_session_rx->ba_drops_replay++;
			return true;
		}

		memcpy(peer->rx_pn[frame_desc->tid], frame_desc->pn, SLSI_RX_PN_LEN);
		ba_session_rx->ba_window[i].sn = frame_desc->sn;
		ba_session_rx->ba_window[i].sent = true;
		return false;
	}

	/* is it a duplicate */
	i = SN_TO_INDEX(ba_session_rx, frame_desc->sn);
	if (ba_session_rx->ba_window[i].sent) {
		SLSI_NET_WARN(dev, "duplicate: tid=%d sn=%d highest_sn=%d, previous PN %pm received PN %pm\n",
			frame_desc->tid,
			frame_desc->sn,
			ba_session_rx->highest_received_sn,
			peer->rx_pn[frame_desc->tid],
			frame_desc->pn);

		ba_session_rx->ba_drops_old++;
		return true;
	}

	/* check if SN is more than highest SN passed so far, if not */
	if (IS_SN_LESS(ba_session_rx->highest_received_sn, frame_desc->sn)) {
		/* check if its PN is more than highest PN paseed to the UL so far */
		if (memcmp(frame_desc->pn, peer->rx_pn[frame_desc->tid], SLSI_RX_PN_LEN) <= 0) {
			SLSI_NET_WARN(dev, "replay detected: tid=%d sn=%d, previous PN %pm received PN %pm\n",
				frame_desc->tid,
				frame_desc->sn,
				peer->rx_pn[frame_desc->tid],
				frame_desc->pn);

			ba_session_rx->ba_drops_replay++;
			return true;
		}
	} else {
		/* check if	its PN is less than the highest PN passed to the UL so far */
		if (memcmp(frame_desc->pn, peer->rx_pn[frame_desc->tid], SLSI_RX_PN_LEN) >= 0) {
			SLSI_NET_WARN(dev, "replay detected: tid=%d sn=%d, previous PN %pm received PN %pm\n",
				frame_desc->tid,
				frame_desc->sn,
				peer->rx_pn[frame_desc->tid],
				frame_desc->pn);

			ba_session_rx->ba_drops_replay++;
			return true;
		}
	}

	/* update current BA window */
	ba_session_rx->ba_window[i].sn = frame_desc->sn;
	ba_session_rx->ba_window[i].sent = true;

	return false;
}

bool slsi_ba_replay_check_pn(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx, struct slsi_ba_frame_desc *frame_desc)
{
	struct slsi_skb_cb *skb_cb;
	u8 i = 0;
	bool ret = false;

	if (!ba_replay_check_enable)
		return false;

	if (frame_desc->flag_old_tdls)
		return false;

	skb_cb = (struct slsi_skb_cb *)frame_desc->signal->cb;

	if (!skb_cb->is_ciphered)
		return false;

	for (i = 0; i < ARRAY_SIZE(replay_check_types); i++) {
		if (replay_check_types[i].option_num == ba_replay_check_option) {
			ret = replay_check_types[i].replay_check_fn(dev, ba_session_rx, frame_desc);
		}
	}
	return ret;
}
