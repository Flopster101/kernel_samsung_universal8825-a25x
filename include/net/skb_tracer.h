/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _SKB_TRACER_H
#define _SKB_TRACER_H

#include <linux/kernel.h>

struct sk_buff;
struct sock;
struct spinlock_t;

struct skb_tracer {
	u64 queue_mask;
	u64 sock_mask;
	u64 skb_mask;

	spinlock_t lock; /* TODO: really need this? */
};

#define ID_TO_MASK(id)		(1 << id)

enum skb_tracer_location {
	STL_TCP_TRANSMIT_SKB 	= ID_TO_MASK(0),
	STL_TCP_WRITE_XMIT	= ID_TO_MASK(1),
	STL___IP_QUEUE_XMIT	= ID_TO_MASK(2),
	STL___IP_LOCAL_OUT	= ID_TO_MASK(3),
	STL_IP6_OUTPUT		= ID_TO_MASK(4),
	STL_IP_OUTPUT		= ID_TO_MASK(5),
	STL_IP6_FINISH_OUTPUT2	= ID_TO_MASK(6),
	STL_IP6_XMIT_01		= ID_TO_MASK(7),
	STL_IP6_XMIT_02		= ID_TO_MASK(8),
};

void skb_tracer_func_trace(const struct sock *sk, struct sk_buff *skb,
				enum skb_tracer_location location);
void skb_tracer_mask_on_skb(struct sock *sk, struct sk_buff *skb);

void skb_tracer_mask(struct sk_buff *skb, u64 mask);
void skb_tracer_unmask(struct sk_buff *skb, u64 mask);

void skb_tracer_return_mask(struct sock *sk);
void skb_tracer_sk_error_report(struct sock *sk);
void skb_tracer_init(struct sock *sk);

#endif	/* _SKB_TRACER_H */
