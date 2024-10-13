/*****************************************************************************
 *
 * Copyright (c) 2012 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_BA_H__
#define __SLSI_BA_H__

#include "dev.h"

#define BA_WINDOW_BOUNDARY 2048

#define SN_TO_INDEX(__ba_session_rx, __sn) (((__sn - __ba_session_rx->start_sn) & 0xFFF) % __ba_session_rx->buffer_size)

#define IS_SN_LESS(sn1, sn2) ((((sn1) - (sn2)) & 0xFFF) > BA_WINDOW_BOUNDARY)

void slsi_handle_blockack(struct net_device *dev, struct slsi_peer *peer,
			  u16 reason_code, u16 user_priority, u16 buffer_size, u16 sequence_number);

int slsi_ba_process_frame(struct net_device *dev, struct slsi_peer *peer,
			  struct sk_buff *skb, u16 sequence_number, u16 tid);

void slsi_ba_update_window(struct net_device *dev,
				  struct slsi_ba_session_rx *ba_session_rx, u16 sequence_number);

void slsi_ba_process_complete(struct net_device *dev, bool ctx_napi);

bool slsi_ba_check(struct slsi_peer *peer, u16 tid);

void slsi_rx_ba_stop_all(struct net_device *dev, struct slsi_peer *peer);

void slsi_rx_ba_init(struct slsi_dev *sdev);

/* replay detection for frames under BA */
void slsi_ba_replay_reset_pn(struct net_device *dev, struct slsi_peer *peer);

void slsi_ba_replay_store_pn(struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb);

void slsi_ba_replay_get_pn(struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb, struct slsi_ba_frame_desc *frame_desc);

bool slsi_ba_replay_check_pn(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx, struct slsi_ba_frame_desc *frame_desc);

#endif
