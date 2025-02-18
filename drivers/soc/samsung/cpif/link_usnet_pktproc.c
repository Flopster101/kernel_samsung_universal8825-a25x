// SPDX-License-Identifier: GPL-2.0
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
#include <net/ip.h>
#include <net/icmp.h>
#include <asm/cacheflush.h>
#include <soc/samsung/shm_ipc.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

static u8 usnet_debug_level;
static DEFINE_IDA(usnet_ida);

static void usnet_umem_unpin_pages(struct usnet_umem *umem)
{
	unsigned int i;

	for (i = 0; i < umem->npgs; i++) {
		struct page *page = umem->pgs[i];

		set_page_dirty_lock(page);
		put_page(page);
	}

	kfree(umem->pgs);
	umem->pgs = NULL;
}

static void usnet_umem_unaccount_pages(struct usnet_umem *umem)
{
	if (umem->user) {
		atomic_long_sub(umem->npgs, &umem->user->locked_vm);
		free_uid(umem->user);
	}
}

static int usnet_umem_pin_pages(struct usnet_umem *umem)
{
	unsigned int gup_flags = FOLL_WRITE;
	long npgs;
	int err;

	umem->pgs = kcalloc(umem->npgs, sizeof(*umem->pgs),
			    GFP_KERNEL | __GFP_NOWARN);
	if (!umem->pgs)
		return -ENOMEM;

	/* TODO CHECK */
	//down_write(&current->mm->mmap_sem);

	mif_err("umem->address=0x%lx, npgs=%d\n", umem->address, umem->npgs);

	npgs = get_user_pages(umem->address, umem->npgs,
			      gup_flags, &umem->pgs[0], NULL);
	/* TODO CHECK */
	//up_write(&current->mm->mmap_sem);

	if (npgs != umem->npgs) {
		if (npgs >= 0) {
			umem->npgs = npgs;
			err = -ENOMEM;
			goto out_pin;
		}
		err = npgs;
		goto out_pgs;
	}
	return 0;

out_pin:
	usnet_umem_unpin_pages(umem);
out_pgs:
	kfree(umem->pgs);
	umem->pgs = NULL;

	mif_err("err=%d\n", err);
	return err;
}

static int usnet_umem_account_pages(struct usnet_umem *umem)
{
	unsigned long lock_limit, new_npgs, old_npgs;

	if (capable(CAP_IPC_LOCK))
		return 0;

	lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;
	umem->user = get_uid(current_user());

	do {
		old_npgs = atomic_long_read(&umem->user->locked_vm);
		new_npgs = old_npgs + umem->npgs;
		if (new_npgs > lock_limit) {
			free_uid(umem->user);
			umem->user = NULL;
			return -ENOBUFS;
		}
	} while (atomic_long_cmpxchg(&umem->user->locked_vm, old_npgs,
				     new_npgs) != old_npgs);
	return 0;
}

static int usnet_umem_reg(struct usnet_umem *umem, struct usnet_umem_reg *mr)
{
	u32 chunk_size = mr->chunk_size, headroom = mr->headroom;
	//unsigned int chunks, chunks_per_page;
	u64 addr = mr->addr, size = mr->len;
	int /*size_chk, */err, i;

	umem->address = (unsigned long)addr;
	umem->chunk_mask = ~((u64)chunk_size - 1);
	umem->size = size;
	umem->headroom = headroom;
	umem->chunk_size_nohr = chunk_size - headroom;
	umem->npgs = size / PAGE_SIZE;
	umem->pgs = NULL;
	umem->user = NULL;
	//INIT_LIST_HEAD(&umem->xsk_list);
	//spin_lock_init(&umem->xsk_list_lock);

	refcount_set(&umem->users, 1);

	err = usnet_umem_account_pages(umem);
	if (err)
		return err;

	err = usnet_umem_pin_pages(umem);
	if (err)
		goto out_account;

	umem->pages = kcalloc(umem->npgs, sizeof(*umem->pages), GFP_KERNEL);
	if (!umem->pages) {
		err = -ENOMEM;
		goto out_pin;
	}

	for (i = 0; i < umem->npgs; i++)
		umem->pages[i].addr = page_address(umem->pgs[i]);

	return 0;

out_pin:
	usnet_umem_unpin_pages(umem);

out_account:
	usnet_umem_unaccount_pages(umem);
	return err;
}

struct usnet_umem *usnet_umem_create(struct usnet_umem_reg *mr)
{
	struct usnet_umem *umem;
	int err;

	umem = kzalloc(sizeof(*umem), GFP_KERNEL);
	if (!umem)
		return ERR_PTR(-ENOMEM);

	err = ida_simple_get(&usnet_ida, 0, 0, GFP_KERNEL);
	if (err < 0) {
		kfree(umem);
		return ERR_PTR(err);
	}
	umem->id = err;

	err = usnet_umem_reg(umem, mr);
	if (err) {
		ida_simple_remove(&usnet_ida, umem->id);
		kfree(umem);
		return ERR_PTR(err);
	}

	return umem;
}

static int pktproc_send_pkt_to_usnet(struct pktproc_queue_usnet *q, int ch, u8 *buffer, int len)
{
	struct pktproc_q_info_usnet *q_info = q->q_info;
	struct pktproc_desc_sktbuf *desc;
	void *target_addr;
	int retry = 2;

	u32 space;

	if (usnet_debug_level & 2)
		mif_info("+++\n");

retry_tx:
	__usnet_cache_flush(q->dev, (void *)q_info, sizeof(struct pktproc_q_info_usnet));

	q->stat.total_cnt++;
	if (!pktproc_check_usnet_q_active(q)) {
		mif_err_limited("Queue LKL not activated\n");
		q->stat.inactive_cnt++;
		return -EACCES;
	}

	space = circ_get_space(q->num_desc, q->done_ptr, q_info->fore_ptr);
	if (space < 1) {
		mif_err_limited("NOSPC num_desc:%d fore:%d done:%d rear:%d\n",
			q->num_desc, q_info->fore_ptr, q->done_ptr, q_info->rear_ptr);
		q->stat.buff_full_cnt++;
		return -ENOSPC;
	}

	if (usnet_debug_level & 1)
		mif_err("pje: temp: space=%d, done=%d, rear=%d, fore=%d\n",
			space, q->done_ptr, q_info->rear_ptr, q_info->fore_ptr);

	desc = &q->desc_ul[q->done_ptr];

	barrier();
	__usnet_cache_flush(q->dev, (void *)desc, sizeof(struct pktproc_desc_sktbuf));

	/* USNET_TODO,
	 * check null for cache coherency issue,
	 * do we still need this?
	 */
	if (desc->cp_data_paddr == 0) {
		mif_err("!!!!!!!!!!!!!!might be cache flush issue(%d)\n", retry);
		retry--;

		if (retry >= 0) {
			//flush_cache_all();
			__usnet_cache_flush(q->dev, (void *)desc, sizeof(struct pktproc_desc_sktbuf));
			goto retry_tx;
		}
		return -ESRCH;
	}

	if (1) {
		/* USNET_TODO:
		 * one more memcpy,
		 * eliminate this in the future
		 */
		target_addr = (void *)(q->q_buff_vbase + (desc->cp_data_paddr - 0x10900000)
				- (NET_SKB_PAD + NET_IP_ALIGN));

		//target_addr = (void *)(q->q_buff_vbase +
		//	(q->done_ptr * q->ppa_usnet->max_packet_size));

		//buffer += (NET_SKB_PAD + NET_IP_ALIGN);

#if IS_ENABLED(CONFIG_CPIF_LATENCY_MEASURE)
		cpif_latency_record((struct cpif_timespec *)((u8 *)target_addr - padding_size));
#endif

		memcpy(target_addr, buffer, len + (NET_SKB_PAD + NET_IP_ALIGN));

		barrier();
		__usnet_cache_flush(q->dev, target_addr, len + (NET_SKB_PAD + NET_IP_ALIGN));

		/* pje_temp, LKL */
		if (usnet_debug_level & 1)
			mif_print_data(target_addr + (NET_SKB_PAD + NET_IP_ALIGN), 16);
	}

	/* USNET_TODO: User's pktproc address can use much more than 36bit. (40bit) */
	if (usnet_debug_level & 1) {
		u64 dest_addr = q->buff_addr_cp + (q->done_ptr * q->ppa_usnet->max_packet_size);
		mif_err("dest_addr=%llx, size=%d\n", dest_addr, len);
	}

	desc->length = len;
	desc->status = PKTPROC_STATUS_DONE | PKTPROC_STATUS_TCPC | PKTPROC_STATUS_IPCS;
	desc->filter_result = 0x9;
	desc->channel_id = ch;

	barrier();
	__usnet_cache_flush(q->dev, (void *)desc, sizeof(struct pktproc_desc_sktbuf));

	if (usnet_debug_level & 1)
		mif_print_data((void *)desc, 16);

	q->done_ptr = circ_new_ptr(q->num_desc, q->done_ptr, 1);

	return len;
}

void usnet_wakeup(struct mem_link_device *mld)
{
	struct link_device *ld = (struct link_device *)mld;
	struct us_net *usnet_obj = ld->usnet_obj;
	int i;

	if (!usnet_obj)
		return;

	for (i = 0; i < US_NET_CLIENTS_MAX; i++) {
		struct us_net_stack *usnet_client = usnet_obj->usnet_clients[i];

		if (usnet_client && atomic_read(&usnet_client->need_wakeup)) {
			atomic_dec(&usnet_client->need_wakeup);
			wake_up(&usnet_client->wq);
		}
	}
}

static int __usnet_check_route(struct us_net *usnet_obj, u8 *data, u16 len)
{
	struct iphdr *ip_hdr;
	struct ipv6hdr *ip6_hdr;
	struct us_net_stack *nstack;
	u8 *old_data;

	int i;
	u16 port = 0;
	__be16 icmp_id = 0;
	u8 protocol = 0;

	if (!usnet_obj || atomic_read(&usnet_obj->clients) == 0 || len == 0) {
		if (usnet_debug_level & 2)
			mif_info("usnet_obj->clients=%d, len=%d\n",
					(usnet_obj ? atomic_read(&usnet_obj->clients) : 0), len);
		goto host_packet;
	}

	/* 1. need to pull? */
	old_data = data += (NET_SKB_PAD + NET_IP_ALIGN);

	/* 2. check ip version */
	if (*data == 0x45) {
		/* ipv4 */
		ip_hdr = (struct iphdr *)data;
		data += (ip_hdr->ihl * 4);
		protocol = ip_hdr->protocol;

		if (ip_hdr->protocol == IPPROTO_TCP) {
			struct tcphdr *tcph = (struct tcphdr *)data;
			port = ntohs(tcph->dest);
		} else if (ip_hdr->protocol == IPPROTO_UDP) {
			struct udphdr *udph = (struct udphdr *)data;
			port = ntohs(udph->dest);
		} else if (ip_hdr->protocol == IPPROTO_ICMP) {
			struct icmphdr *icmph = (struct icmphdr *)data;
			icmp_id = icmph->un.echo.id;
		}
	} else if ((*data & 0x60) == 0x60) {
		/* ipv6 */
		ip6_hdr = (struct ipv6hdr *)data;
		data += sizeof(struct ipv6hdr);
		protocol = ip6_hdr->nexthdr;

		if (ip6_hdr->nexthdr == NEXTHDR_TCP) {
			struct tcphdr *tcph = (struct tcphdr *)data;
			port = ntohs(tcph->dest);
		} else if (ip6_hdr->nexthdr == NEXTHDR_UDP) {
			struct udphdr *udph = (struct udphdr *)data;
			port = ntohs(udph->dest);
		} else if (ip6_hdr->nexthdr == NEXTHDR_ICMP) {
			struct icmp6hdr *icmph = (struct icmp6hdr *)data;
			icmp_id = icmph->icmp6_identifier;
		}

	} else {
		/* USNET_TODO:
		 * icmp needed to be treated at Userspace net, here
		 */
		 if (usnet_debug_level & 2)
			mif_info("host packet: port=%d\n", port);

		goto host_packet;
	}

	for (i = 0; i < US_NET_CLIENTS_MAX; i++) {
		nstack = usnet_obj->usnet_clients[i];

		if (!nstack)
			continue;

		if (usnet_debug_level & 2)
			mif_info("(%d,%d), port=%d\n",
				nstack->reserved_port_range[US_NET_PORT_MIN],
				nstack->reserved_port_range[US_NET_PORT_MAX],
				port);

		if (usnet_debug_level & 2)
			mif_print_data(old_data , 32);

		if (protocol == NEXTHDR_ICMP ||
			protocol == IPPROTO_ICMP) {

			mif_info("icmp_id=%d\n", icmp_id);
			return i;
		}

		if (nstack->reserved_port_range[US_NET_PORT_MIN] <= port &&
			port <= nstack->reserved_port_range[US_NET_PORT_MAX]) {
			/* Should be go up to Userspace network.
			 * return it's index
			 */
			return i;
		}
	}

host_packet:
	return -1;
}

int usnet_packet_route(struct mem_link_device *mld, int ch, u8 *data, u16 len)
{
	struct link_device *ld = (struct link_device *)mld;
	struct us_net *usnet_obj = ld->usnet_obj;
	int us_net_idx = __usnet_check_route(usnet_obj, data, len);
	int ret = 0;

	if (0 <= us_net_idx && us_net_idx < US_NET_CLIENTS_MAX) {
		struct us_net_stack *usnet_client = usnet_obj->usnet_clients[us_net_idx];
		struct pktproc_queue_usnet *q = (usnet_client ? usnet_client->q : NULL);

		/* client doesn't exist, ignore */
		if (!usnet_client || !q) {
			if (usnet_debug_level & 2)
				mif_info("usnet queue doesn't exist\n");
			return -ESRCH;
		}

		/* USNET_TODO:
		 * Userspace network could be more than one object.
		 * us_net_idx */

		ret = pktproc_send_pkt_to_usnet(q, ch, data, len);
		if (ret > 0) {
			/* need to wake up */
			wake_up(&usnet_client->wq);
		} else if (ret < 0) {
			mif_err("failed to send packet to usnet(%d)\n", ret);
			ret = -ESRCH;
		}
	}

	/* if 0, then fall back to legacy rx flow */

	return ret;
}


#if 0 // USENET_TODO:
/*
 * Debug
 */
static ssize_t region_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);
	struct link_device *ld = get_current_link(mc->iod);
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct pktproc_adaptor_ul *ppa_ul = &mld->pktproc_ul;
	struct pktproc_info_ul *info_ul =
		(struct pktproc_info_ul *)ppa_ul->info_vbase;

	ssize_t count = 0;
	int i;

	count += scnprintf(&buf[count], PAGE_SIZE - count, "CP base:0x%08lx\n", ppa_ul->cp_base);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "Num of queue:%d\n", ppa_ul->num_queue);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "HW cache coherency:%d\n",
			ppa_ul->use_hw_iocc);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	count += scnprintf(&buf[count], PAGE_SIZE - count, "CP quota:%d\n", info_ul->cp_quota);

	for (i = 0; i < ppa_ul->num_queue; i++) {
		struct pktproc_queue_ul *q = ppa_ul->q[i];

		if (!pktproc_check_ul_q_active(ppa_ul, q->q_idx)) {
			count += scnprintf(&buf[count], PAGE_SIZE - count,
					"Queue %d is not active\n", i);
			continue;
		}

		count += scnprintf(&buf[count], PAGE_SIZE - count, "Queue%d\n", i);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  num_desc:%d(0x%08x)\n",
				q->q_info->num_desc, q->q_info->num_desc);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  cp_desc_pbase:0x%08x\n",
				q->q_info->cp_desc_pbase);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  desc_size:0x%08x\n",
				q->desc_size);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  cp_buff_pbase:0x%08x\n",
				q->q_info->cp_buff_pbase);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  q_buff_size:0x%08x\n",
				q->q_buff_size);
	}

	return count;
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);
	struct link_device *ld = get_current_link(mc->iod);
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct pktproc_adaptor_ul *ppa_ul = &mld->pktproc_ul;
	ssize_t count = 0;
	int i;

	for (i = 0; i < ppa_ul->num_queue; i++) {
		struct pktproc_queue_ul *q = ppa_ul->q[i];

		if (!pktproc_check_ul_q_active(ppa_ul, q->q_idx)) {
			count += scnprintf(&buf[count], PAGE_SIZE - count,
					"Queue %d is not active\n", i);
			continue;
		}

		count += scnprintf(&buf[count], PAGE_SIZE - count, "Queue%d\n", i);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  num_desc:%d\n",
				q->num_desc);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  fore/rear:%d/%d\n",
				q->q_info->fore_ptr, q->q_info->rear_ptr);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  pass:%lld\n",
				q->stat.pass_cnt);
		count += scnprintf(&buf[count], PAGE_SIZE - count,
				"  fail: buff_full:%lld inactive:%lld\n",
				q->stat.buff_full_cnt, q->stat.inactive_cnt);
		count += scnprintf(&buf[count], PAGE_SIZE - count, "  total:%lld\n",
				q->stat.total_cnt);
	}

	return count;
}

static DEVICE_ATTR_RO(region);
static DEVICE_ATTR_RO(status);

static struct attribute *pktproc_usnet_attrs[] = {
	&dev_attr_region.attr,
	&dev_attr_status.attr,
	NULL,
};

static const struct attribute_group pktproc_usnet_group = {
	.attrs = pktproc_usnet_attrs,
	.name = "pktproc_usnet",
};
#endif

int pktproc_usnet_update_fore_ptr(struct pktproc_queue_usnet *q, u32 count)
{
	unsigned int last_ptr;
	unsigned int rear_ptr;

	last_ptr = q->q_info->rear_ptr;
	rear_ptr = circ_new_ptr(q->num_desc, last_ptr, count);
	q->q_info->rear_ptr = rear_ptr;

	/* ensure the fore_ptr ordering */
	smp_mb();
	__usnet_cache_flush(q->dev, (void *)q->q_info, sizeof(struct pktproc_q_info_usnet));

	return 0;
}


/*
 * Create PktProc buffer sharing for userspace network
 */
static int pktproc_get_info_from_usnet(
		struct pktproc_adaptor_usnet *ppa_usnet,
		struct pktproc_adaptor_info *ppa)
{
	ppa_usnet->cp_quota = 64;	// USNET_TODO:

	ppa_usnet->cp_base = ppa->cp_base;
	ppa_usnet->num_queue = ppa->num_queue;
	ppa_usnet->max_packet_size = 2048;
	ppa_usnet->use_hw_iocc = 0;
	ppa_usnet->info_desc_rgn_cached = 0;
	ppa_usnet->buff_rgn_cached = 0;
	//ppa_usnet->padding_required = 1;
	ppa_usnet->info_rgn_offset = ppa->info_rgn_offset;
	ppa_usnet->info_rgn_size = ppa->info_rgn_size;
	ppa_usnet->desc_rgn_offset = ppa->desc_rgn_offset;
	ppa_usnet->desc_rgn_size = ppa->desc_rgn_size;
	ppa_usnet->buff_rgn_offset = ppa->data_rgn_offset;

	mif_info("cp_base:0x%08lx num_queue:%d max_packet_size:%d iocc:%d\n",
		ppa_usnet->cp_base, ppa_usnet->num_queue, ppa_usnet->max_packet_size, ppa_usnet->use_hw_iocc);
	mif_info("info/desc rgn cache: %d buff rgn cache: %d padding_required:%d\n",
		ppa_usnet->info_desc_rgn_cached, ppa_usnet->buff_rgn_cached, ppa_usnet->padding_required);
	mif_info("info_rgn 0x%08lx 0x%08lx desc_rgn 0x%08lx 0x%08lx buff_rgn 0x%08lx\n",
		ppa_usnet->info_rgn_offset, ppa_usnet->info_rgn_size,	ppa_usnet->desc_rgn_offset,
		ppa_usnet->desc_rgn_size, ppa_usnet->buff_rgn_offset);

	return 0;
}

static void *get_nc_region(struct page **pages, unsigned int num_pages, unsigned long offset)
{
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	void *v_addr;

	if (num_pages == 0 || !pages)
		return NULL;

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	if (v_addr == NULL)
		mif_err("%s: Failed to vmap pages\n", __func__);

	mif_err("v_addr=0x%llx, offset=0x%lx\n", (u64)v_addr, offset);

	return (void *)((u64)v_addr + offset);
}

int pktproc_create_usnet(struct device *dev, struct usnet_umem *umem,
		struct pktproc_adaptor_info *pai,
		struct us_net_stack *usnet_client)
{
	struct mem_link_device *mld = usnet_client->usnet_obj->mld;
	struct pktproc_adaptor_usnet *ppa_usnet = &mld->pktproc_usnet;
	struct pktproc_info_v2 *ul_info;
	u32 buff_size, buff_size_by_q;
	int i;
	int ret;
	unsigned long memaddr = pai->addr;
	u32 memsize = pai->len;

	/* Get info */
	ret = pktproc_get_info_from_usnet(ppa_usnet, pai);
	if (ret != 0) {
		mif_err("pktproc_get_info_from_usnet() error %d\n", ret);
		return ret;
	}

	/* Get base addr */
	mif_info("memaddr:0x%lx memsize:0x%08x\n", memaddr, memsize);
	if (ppa_usnet->info_desc_rgn_cached)
		ppa_usnet->info_vbase = phys_to_virt(memaddr);
	else {
		unsigned long addr_offset = (memaddr & (PAGE_SIZE - 1));

		ppa_usnet->info_vbase = get_nc_region(umem->pgs, umem->npgs, addr_offset);
		ppa_usnet->info_vbase_org = (void *)((u64)ppa_usnet->info_vbase - addr_offset);
	}
	if (!ppa_usnet->info_vbase) {
		mif_err("ppa->info_vbase error\n");
		return -ENOMEM;
	}
	ppa_usnet->desc_vbase = ppa_usnet->info_vbase + ppa_usnet->info_rgn_size;
	//memset(ppa_usnet->info_vbase, 0,
	//		ppa_usnet->info_rgn_size + ppa_usnet->desc_rgn_size);
	mif_info("info + desc size:0x%08lx\n",
			ppa_usnet->info_rgn_size + ppa_usnet->desc_rgn_size);
	buff_size = memsize - (ppa_usnet->info_rgn_size + ppa_usnet->desc_rgn_size);
	buff_size_by_q = buff_size / ppa_usnet->num_queue;
	if (ppa_usnet->buff_rgn_cached)
		ppa_usnet->buff_vbase =
			phys_to_virt(memaddr + ppa_usnet->buff_rgn_offset);
	else
		ppa_usnet->buff_vbase = ppa_usnet->info_vbase + ppa_usnet->buff_rgn_offset;

	mif_info("Total buffer size:0x%08x Queue:%d Size by queue:0x%08x\n",
					buff_size, ppa_usnet->num_queue,
					buff_size_by_q);

	mif_err("pje: info_vbase=0x%llx, desc_vbase=0x%llx, buff_vbase=0x%llx\n",
			(u64)ppa_usnet->info_vbase, (u64)ppa_usnet->desc_vbase, (u64)ppa_usnet->buff_vbase);

	/* LKL, v2 */
	ul_info = (struct pktproc_info_v2 *)ppa_usnet->info_vbase;

	barrier();
	__usnet_cache_flush(dev, (void *)ul_info, sizeof(struct pktproc_info_v2));

	/* debug level */
	usnet_debug_level = ul_info->reserved;
	mif_info("debug level = 0x%x\n", usnet_debug_level);

	//ul_info->num_queues = ppa_usnet->num_queue;

	/* Create queue */
	for (i = 0; i < ppa_usnet->num_queue; i++) {
		struct pktproc_queue_usnet *q;

		mif_info("Queue %d\n", i);

		ppa_usnet->q = kzalloc(sizeof(struct pktproc_queue_usnet),
				GFP_ATOMIC);
		if (ppa_usnet->q == NULL) {
			mif_err_limited("kzalloc() error %d\n", i);
			ret = -ENOMEM;
			goto create_error;
		}
		q = ppa_usnet->q;

		q->dev = dev;

		/* Info region */
		q->ul_info = ul_info;
		q->q_info = (struct pktproc_q_info_usnet *)&q->ul_info->q_info[i];

		q->q_buff_vbase = ppa_usnet->buff_vbase + (i * buff_size_by_q);
		q->cp_buff_pbase = ppa_usnet->cp_base +
			ppa_usnet->buff_rgn_offset + (i * buff_size_by_q);
		q->q_info->cp_buff_pbase = q->cp_buff_pbase;
		q->q_buff_size = buff_size_by_q;
		q->num_desc = buff_size_by_q / ppa_usnet->max_packet_size;
		//q->q_info->num_desc = q->num_desc;
		q->desc_ul = ppa_usnet->desc_vbase +
			(i * sizeof(struct pktproc_desc_sktbuf) * q->num_desc);
		q->cp_desc_pbase = ppa_usnet->cp_base +
			ppa_usnet->desc_rgn_offset +
			(i * sizeof(struct pktproc_desc_sktbuf) * q->num_desc);
		q->q_info->cp_desc_pbase = q->cp_desc_pbase;
		q->desc_size = sizeof(struct pktproc_desc_sktbuf) * q->num_desc;
		q->buff_addr_cp = ppa_usnet->cp_base + ppa_usnet->buff_rgn_offset +
			(i * buff_size_by_q);

		if ((q->cp_desc_pbase + q->desc_size) > q->cp_buff_pbase) {
			mif_err("Descriptor overflow:0x%08llx 0x%08x 0x%08llx\n",
				q->cp_desc_pbase, q->desc_size, q->cp_buff_pbase);
			goto create_error;
		}

		spin_lock_init(&q->lock);

		q->q_idx = i;
		q->mld = mld;
		q->ppa_usnet = ppa_usnet;
		q->usnet_client = usnet_client;
		usnet_client->q = q;

		/* LKL, lkl will update this */
		//q->q_info->fore_ptr = 0;
		//q->q_info->rear_ptr = 0;
		mif_info("num_desc=%d, %d, %d\n", q->q_info->num_desc, pai->desc_num, q->num_desc);

		q->num_desc = pai->desc_num; // USNET_TODO: LKL,

		q->fore_ptr = &q->q_info->fore_ptr;
		q->rear_ptr = &q->q_info->rear_ptr;
		q->done_ptr = *q->rear_ptr;

		mif_info("num_desc:%d cp_desc_pbase:0x%08llx desc_size:0x%08x\n",
			q->num_desc, q->cp_desc_pbase, q->desc_size);
		mif_info("buff_offset:0x%08llx buff_size:0x%08x\n",
			q->cp_buff_pbase, q->q_buff_size);

		mif_info("fore_ptr: 0x%llx, rear_ptr: 0x%llx\n",
			(u64)&q->q_info->fore_ptr,
			(u64)&q->q_info->rear_ptr);


		mif_info("fore_ptr: %d, rear_ptr: %d\n", *q->fore_ptr, *q->rear_ptr);

		/* USNET_TODO: */
		atomic_set(&q->active, 1);

		//_us_net_desc_cache_flush(q);
	}

	//__flush_dcache_area((void *)ul_info, sizeof(struct pktproc_info_v2));

	print_hex_dump(KERN_ERR, "mif: ", DUMP_PREFIX_ADDRESS,
			   16, 1, (void *)ul_info, 0x80, false);

#if 0 // USENET_TODO:
	ret = sysfs_create_group(&dev->kobj, &pktproc_usnet_group);
	if (ret != 0) {
		mif_err("sysfs_create_group() error %d\n", ret);
		//goto create_error;
	}
#endif
	return 0;

create_error:
	//for (i = 0; i < ppa_usnet->num_queue; i++)
		kfree(ppa_usnet->q);

	if (!ppa_usnet->info_desc_rgn_cached && ppa_usnet->info_vbase)
		vunmap(ppa_usnet->info_vbase);
//	if (!ppa_usnet->buff_rgn_cached && ppa_usnet->buff_vbase)
//		vunmap(ppa_usnet->buff_vbase);

	return ret;
}

int pktproc_destroy_usnet(struct us_net_stack *usnet_client)
{
	struct usnet_umem *umem = usnet_client->umem;

	if (!usnet_client->q)
		return 0;

	atomic_set(&usnet_client->q->active, 0);

	// USNET_TODO:
	//sysfs_remove_group(&dev->kobj, &pktproc_usnet_group);

	kfree(usnet_client->q);
	usnet_client->q = NULL;

	if (umem) {
		usnet_umem_unpin_pages(umem);
		usnet_umem_unaccount_pages(umem);
		kfree(umem);

		usnet_client->umem = NULL;
	}

	return 0;

}

