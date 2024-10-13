/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021, Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINK_USNET_PKTPROC_H__
#define __LINK_USNET_PKTPROC_H__

#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include "link_device_memory.h"

#if !IS_ENABLED(CONFIG_CP_PKTPROC_UL)
/* Numbers */
#define PKTPROC_HIPRIO_UL	0
#define PKTPROC_NORM_UL		1
#define PKTPROC_MAX_QUEUE_UL	2

/* Statistics */
struct pktproc_statistics_ul {
	/* count of trials to write a packet to pktproc UL */
	u64 total_cnt;
	/* number of times failed to write a packet due to full buffer */
	u64 buff_full_cnt;
	/* number of times failed to write a packet due to inactive q */
	u64 inactive_cnt;
	/* number of times succeed to write a packet to pktproc UL */
	u64 pass_cnt;
};
#endif

/* Q_info
 * same as pktproc_q_info_ul
 */
struct pktproc_q_info_usnet {
	u32 cp_desc_pbase;
	u32 num_desc;
	u32 cp_buff_pbase;
	u32 fore_ptr;
	u32 rear_ptr;
} __packed;

/* Logical view for each queue */
struct pktproc_queue_usnet {
	struct device *dev;
	u32 q_idx;
	atomic_t active; /* activated when pktproc ul init */
	atomic_t busy; /* used for flow control */
	spinlock_t lock;

	struct mem_link_device *mld;
	struct pktproc_adaptor_usnet *ppa_usnet;
	struct us_net_stack *usnet_client;

	atomic_t need_wakeup;

	u32 *fore_ptr; /* indicates the last last-bit raised desc pointer */
	u32 done_ptr; /* indicates the last packet written by AP */
	u32 *rear_ptr; /* indicates the last desc read by CP */

	/* Store */
	u64 cp_desc_pbase;
	u32 num_desc;
	u64 cp_buff_pbase;

	struct pktproc_info_v2 *ul_info;
	struct pktproc_q_info_usnet *q_info;	/* Pointer to q_info of info_v */
	struct pktproc_desc_sktbuf *desc_ul;

	u32 desc_size;
	u64 buff_addr_cp; /* base data address value for cp */
	u32 quota_usage;

	/* Pointer to data buffer */
	u8 __iomem *q_buff_vbase;
	u32 q_buff_size;

	/* Statistics */
	struct pktproc_statistics_ul stat;

	/* LKL */
	struct usnet_umem *umem;
};

/* PktProc adaptor for lkl ul */
struct pktproc_adaptor_usnet {
	unsigned long cp_base;		/* CP base address for pktproc */
	unsigned long info_rgn_offset;	/* Offset of info region */
	unsigned long info_rgn_size;	/* Size of info region */
	unsigned long desc_rgn_offset;	/* Offset of descriptor region */
	unsigned long desc_rgn_size;	/* Size of descriptor region */
	unsigned long buff_rgn_offset;	/* Offset of data buffer region */

	u32 num_queue;		/* Number of queue */
	u32 max_packet_size;	/* packet size pktproc UL can hold */
	u32 cp_quota;	/* max number of buffers cp allows us to transfer */
	bool use_hw_iocc;	/* H/W IO cache coherency */
	bool info_desc_rgn_cached;
	bool buff_rgn_cached;
	bool padding_required;	/* requires extra length. (s5123 EVT1 only) */

	void *info_vbase_org;
	void *info_vbase;	/* I/O region for information */
	void *desc_vbase;	/* I/O region for descriptor */
	void *buff_vbase;	/* I/O region for data buffer */
	struct pktproc_queue_usnet *q;/* Logical queue */
};

struct usnet_umem_reg {
	__u64 addr; /* Start of packet data area */
	__u64 len; /* Length of packet data area */
	__u32 chunk_size;
	__u32 headroom;
	__u32 type;
} __packed;

/* pktproc information from userspace */
struct pktproc_adaptor_info {
	/* from xdp_umem_reg*/
	__u64 addr; /* Start of packet data area */
	__u64 len; /* Length of packet data area */
	__u32 chunk_size;
	__u32 headroom;
	__u32 type;

	/* from pktproc_adaptor */
	__u64 cp_base;		/* CP base address for pktproc */
	__u32 info_rgn_offset;	/* Offset of info region */
	__u32 info_rgn_size;	/* Size of info region */
	__u32 desc_rgn_offset;	/* Offset of descriptor region */
	__u32 desc_rgn_size;	/* Size of descriptor region */
	__u32 data_rgn_offset;	/* Offset of data buffer region */
	__u32 num_queue;

	__u32 desc_num;

	__u8 use_hw_iocc;

	/* route info */
	__u16 port_min;
	__u16 port_max;
} __packed;

struct usnet_port_range {
	__u16 port_min;
	__u16 port_max;
} __packed;

/* Userspace network  */
struct usnet_umem_page {
	void *addr;
	dma_addr_t dma;
};

struct usnet_umem {
	struct usnet_umem_page *pages;
	u64 chunk_mask;
	u64 size;
	u32 headroom;
	u32 chunk_size_nohr;
	struct user_struct *user;
	unsigned long address;
	refcount_t users;

	struct page **pgs;
	u32 npgs;

	int id;
};

static inline char *usnet_umem_get_data(struct usnet_umem *umem, u64 addr)
{
	return umem->pages[addr >> PAGE_SHIFT].addr + (addr & (PAGE_SIZE - 1));
}

static inline dma_addr_t usnet_umem_get_dma(struct usnet_umem *umem, u64 addr)
{
	return umem->pages[addr >> PAGE_SHIFT].dma + (addr & (PAGE_SIZE - 1));
}

int usnet_packet_route(struct mem_link_device *mld, int ch, u8 *data, u16 len);
void usnet_wakeup(struct mem_link_device *mld);

struct usnet_umem *usnet_umem_create(struct usnet_umem_reg *mr);
int pktproc_create_usnet(struct device *dev, struct usnet_umem *umem,
		struct pktproc_adaptor_info *pai,
		struct us_net_stack *usnet_client);

int pktproc_destroy_usnet(struct us_net_stack *usnet_client);

static inline int pktproc_check_usnet_q_active(struct pktproc_queue_usnet *q)
{
	if (!q)
		return 0;

	return atomic_read(&q->active);
}

int pktproc_usnet_update_fore_ptr(struct pktproc_queue_usnet *q, u32 count);

static inline void __usnet_cache_flush(struct device *dev, void *data, u32 len)
{
#if 0 // (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	dma_addr_t dma_addr;
	dma_addr = dma_map_page_attrs(dev, virt_to_page(data), offset_in_page(data),
				len, DMA_BIDIRECTIONAL, 0);

	dma_unmap_single(dev, dma_addr, len, DMA_BIDIRECTIONAL);

//	dma_sync_single_for_device(dev, virt_to_phys(data), len, DMA_FROM_DEVICE);
#else
	extern void cpif_flush_dcache_area(void *addr, size_t len);

	cpif_flush_dcache_area(data, len);
#endif
}

#endif /* __LINK_USNET_PKTPROC_H__ */

