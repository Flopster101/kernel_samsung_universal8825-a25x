// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Samsung Electronics.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/module.h>
#include <trace/events/napi.h>
#include <net/ip.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/netdevice.h>
#include <net/tcp.h>
#include <linux/uio.h>

#include <soc/samsung/exynos-modem-ctrl.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_dump.h"
#if IS_ENABLED(CONFIG_MODEM_IF_LEGACY_QOS)
#include "cpif_qos_info.h"
#endif
#include "link_usnet_pktproc.h"
#include "modem_klat.h"

/* Enables when it needs to restore pktproc ptrs back before loopback. */
/* #define FEATURE_LOOPBACK_RESTORE_Q_PTR */

#define USNET_MISC_DEV			"usnet"
#define US_NET_CLIENT_PORT_BASE		61000
#define US_NET_CLIENT_PORT_SIZE		1000

static struct us_net *usnet;

#define iov_for_each(iov, iter, start)				\
	if (!((start).type & (ITER_BVEC | ITER_PIPE)))		\
	for (iter = (start);					\
	     (iter).count &&					\
	     ((iov = iov_iter_iovec(&(iter))), 1);		\
	     iov_iter_advance(&(iter), (iov).iov_len))

static ssize_t usnet_chr_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *filp = iocb->ki_filp;
	struct us_net_iod *usnet_iod = (struct us_net_iod *)filp->private_data;
	struct us_net *usnet_obj = usnet_iod->usnet_obj;
	struct us_net_stack *usnet_client = usnet_iod->usnet_client;
	struct link_device *ld = (struct link_device *)(usnet_obj->mld);

	struct io_device *iod;
	struct net_device *ndev;
	struct modem_ctl *mc;
	unsigned int count = 0;

	unsigned int tx_bytes;
	int total_count = 0;
	int ret;

	struct iov_iter *base = from, iter;
	struct iovec iov;
	struct mif_iov *p_iov;
	int i = 0;

	if (!usnet_client || !ld->send_iov)
		return -EPERM;

	iod = usnet_client->iod;
	ndev = iod->ndev;
	mc = iod->mc;

	iov_for_each(iov, iter, *base) {
		/* this iov will be freed at
		 * tx_iov_frames_to_rb, sbd_iov_tx_func
		 * after sbd_iov_pio_tx
		 */
		p_iov = kmalloc(sizeof(struct mif_iov), GFP_KERNEL);

		if (!p_iov) {
			/* USNET_TODO: in case of allocation failure,
			 * 1. drop?
			 * 2. retry?
			 */

			continue;
		}
retry:
		p_iov->iov = iov;
		p_iov->skb = NULL;
		p_iov->ch = iod->ch;
		count = p_iov->iov.iov_len;
		tx_bytes = count;

		INIT_LIST_HEAD(&p_iov->list);

		ret = ld->send_iov(ld, iod, p_iov);

		if (unlikely(ret < 0)) {
			static DEFINE_RATELIMIT_STATE(_rs, HZ, 100);

			if (ret != -EBUSY) {
				mif_err_limited("%s->%s: ERR! %s->send fail:%d (tx_bytes:%d len:%d)\n",
					iod->name, mc->name, ld->name, ret,
					tx_bytes, count);
				goto drop;
			}

			/* do 100-retry for every 1sec */
			if (__ratelimit(&_rs))
				goto retry;
			goto drop;
		}

		if (ret != tx_bytes) {
			mif_info("%s->%s: WARN! %s->send ret:%d (tx_bytes:%d len:%d)\n",
				iod->name, mc->name, ld->name, ret, tx_bytes, count);
		}

		ndev->stats.tx_packets++;
		ndev->stats.tx_bytes += count;

		total_count += count;

		i++;
	}

	return total_count;
drop:
	ndev->stats.tx_dropped++;
	return total_count;

}

static bool usnet_queue_empty(struct pktproc_queue_usnet *q)
{
	return circ_get_usage(q->q_info->num_desc,
				q->done_ptr, q->q_info->rear_ptr) == 0;
}

static bool usnet_q_active(struct pktproc_queue_usnet *q)
{
	return pktproc_check_usnet_q_active(q);
}

static __poll_t usnet_chr_poll(struct file *filp, poll_table *wait)
{
	struct us_net_iod *usnet_iod = (struct us_net_iod *)filp->private_data;
	struct us_net *usnet_obj;
	struct us_net_stack *usnet_client;
	struct modem_ctl *mc;
	struct io_device *iod;
	struct pktproc_queue_usnet *q;

	if (!usnet_iod)
		return POLLERR;

	usnet_obj = usnet_iod->usnet_obj;
	usnet_client = usnet_iod->usnet_client;

	if (!usnet_client || !usnet_client->q)
		return POLLERR;

	q   = usnet_client->q;
	iod = usnet_client->iod;
	mc  = usnet_obj->mc;

	if (!usnet_q_active(q))
		return POLLERR;

	if (usnet_queue_empty(q))
		poll_wait(filp, &usnet_client->wq, wait);

	switch (mc->phone_state) {
	case STATE_BOOTING:
	case STATE_ONLINE:
		if (!usnet_queue_empty(q)) {
			int rcvd = circ_get_usage(q->q_info->num_desc,
				q->done_ptr, q->q_info->rear_ptr);

			pktproc_usnet_update_fore_ptr(q , rcvd);

			/* usnet need POLLPRI, POLLRDNORM; */
			return POLLIN | POLLPRI;
		}
		else /* wq is waken up without rx, return for wait */
			return 0;
		/* fall through, if sim_state has been changed */
	case STATE_CRASH_EXIT:
	case STATE_CRASH_RESET:
	case STATE_NV_REBUILDING:
	case STATE_CRASH_WATCHDOG:
		{
			mif_err("%s: %s.state == %s\n", iod->name, mc->name,
				mc_state(mc));

			/* give delay to prevent infinite sys_poll call from
			 * select() in APP layer without 'sleep' user call takes
			 * almost 100% cpu usage when it is looked up by 'top'
			 * command.
			 */
			msleep(20);
		}
		break;

	default:
		break;
	}

	return 0;
}

static int __usnet_get_unused_client(struct us_net *usnet_obj)
{
	int i;
	struct us_net_stack *usnet_client;

	for (i = 0; i < US_NET_CLIENTS_MAX; i++) {
		usnet_client = usnet_obj->usnet_clients[i];

		if (!usnet_client)
			return i;
	}
	return -1;
}

static long usnet_chr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct us_net_iod *usnet_iod = (struct us_net_iod *)filp->private_data;
	struct us_net *usnet_obj = usnet_iod->usnet_obj;
	struct us_net_stack *usnet_client = usnet_iod->usnet_client;

	mif_info("cmd: 0x%x by %s(%d)\n", cmd, current->comm, current->pid);

	switch (cmd) {
	case IOCTL_USNET_PKTPROC_INFO:
	{
		struct pktproc_adaptor_info pai;
		struct usnet_umem_reg mr;
		struct usnet_umem *umem;
		int err;
		int client_idx;

		if (usnet_client) {
			mif_err("ERR! already created by %s(%d)\n", current->comm, current->pid);
			return -EEXIST;
		}

		if (copy_from_user(&pai, (void __user *)arg, sizeof(pai))) {
			mif_err("ERR! can't get client info\n");
			return -EFAULT;
		}

		if (atomic_read(&usnet_obj->clients) >= US_NET_CLIENTS_MAX) {
			mif_err("ERR! can't assign new usnet client\n");
			return -EBUSY;
		}

		if (!usnet_obj->inet_pdn_iod) {
			mif_err("ERR! there's no inet pdn iod\n");
			return -ENODEV;
		}

		client_idx = __usnet_get_unused_client(usnet_obj);

		if (client_idx < 0) {
			mif_err("ERR! no empty client slot\n");
			return -EBUSY;
		}

		usnet_client = (struct us_net_stack *)kzalloc(sizeof(struct us_net_stack),
					GFP_KERNEL);

		if (!usnet_client) {
			mif_err("ERR! can't alloc client\n");
			return -ENOMEM;
		}

		atomic_inc(&usnet_obj->clients);
		usnet_obj->usnet_clients[client_idx] = usnet_client;

		/* USNET_TODO:
		 * temp. create umem */
		mr.addr = pai.addr;
		mr.len = pai.len;
		mr.chunk_size = pai.chunk_size;
		mr.headroom = pai.headroom;
		mr.type = pai.type;

		umem = usnet_umem_create(&mr);
		if (IS_ERR(umem)) {
			kfree(usnet_client);
			atomic_dec(&usnet_obj->clients);
			mif_err("ERR! can't create usnet memory(%ld)\n", PTR_ERR(umem));
			return -ENOMEM;
		}

		usnet_client->umem = umem;
		usnet_client->usnet_obj = usnet_obj;

		smp_wmb();

		err = pktproc_create_usnet(usnet_obj->dev, umem, &pai, usnet_client);
		if (err < 0) {
			kfree(usnet_client);
			usnet_obj->usnet_clients[client_idx] = NULL;
			atomic_dec(&usnet_obj->clients);

			/* USNET_TODO: need destroy umem?? */

			mif_err("ERR! pktproc_create_ul() error %d\n", err);
			return -EINVAL;
		}

		usnet_client->iod = usnet_obj->inet_pdn_iod;
		usnet_client->user_pid = current->pid;
		usnet_client->us_net_index = client_idx;
		usnet_client->reserved_port_range[US_NET_PORT_MIN] =
				US_NET_CLIENT_PORT_BASE +
				US_NET_CLIENT_PORT_SIZE * client_idx;
		usnet_client->reserved_port_range[US_NET_PORT_MAX] =
				usnet_client->reserved_port_range[US_NET_PORT_MIN] +
				US_NET_CLIENT_PORT_SIZE - 1;

		usnet_client->usnet_iod = usnet_iod;
		usnet_iod->usnet_client = usnet_client;

		init_waitqueue_head(&usnet_client->wq);

		mif_info("pktproc usnet created (idx=%d, ch=%d) by %s(%d)\n",
				client_idx, usnet_client->iod->ch,
				current->comm, current->pid);
		break;
	}

	case IOCTL_USNET_GET_PORT_RANGE:
	{
		struct usnet_port_range range;

		if (!usnet_client) {
			mif_err("no usnet_client created by %s(%d)\n",
					current->comm, current->pid);
			return -ENODEV;
		}

		range.port_min = usnet_client->reserved_port_range[US_NET_PORT_MIN];
		range.port_max = usnet_client->reserved_port_range[US_NET_PORT_MAX];

		mif_info("GET_PORT_RANGE(%d): value(%d, %d)\n",
				usnet_client->us_net_index,
				range.port_min, range.port_max);

		if (copy_to_user((void __user *)arg, &range, sizeof(range)))
			return -EFAULT;
		break;
	}

	case IOCTL_USNET_GET_XLAT_INFO:
	{
		if (copy_to_user((void __user *)arg, &klat_obj, sizeof(struct klat))) {
			mif_err("failed to get xlat_info\n");
			return -EFAULT;
		}

		mif_info("copied xlat_info to user\n");
		break;
	}

	case IOCTL_USNET_SET_LOOPBACK:
	{
		int idx = usnet_client->us_net_index;
		struct us_net_loopback_data *usnet_lb = &usnet_obj->loopback[idx];
#ifdef FEATURE_LOOPBACK_RESTORE_Q_PTR
		struct us_net_loopback_pktproc_pointers *pktproc_pointers =
			&usnet_obj->pktproc_pointers;
#endif
		struct mem_link_device *mld = usnet_obj->mld;
		struct pktproc_queue *q = mld->pktproc.q[0];

		if (copy_from_user(usnet_lb,
					(void __user *)arg,
					sizeof(struct us_net_loopback))) {
			mif_err("failed to get loopback info\n");
			return -EFAULT;
		}
		/*
		 * EXECUTE NORMALLY WHEN clients == 1
		 */

		usnet_lb->ch = usnet_client->iod->ch;;

		if (usnet_lb->do_loopback)
			mif_info("lkl loopback test\n");

#ifdef FEATURE_LOOPBACK_RESTORE_Q_PTR
		pktproc_pointers->fore_ptr = *q->fore_ptr;
		pktproc_pointers->rear_ptr = *q->rear_ptr;
		pktproc_pointers->done_ptr = q->done_ptr;
#endif

		q->do_loopback = usnet_lb->do_loopback;
		q->usnet_lb = usnet_lb;

		skb_queue_head_init(&usnet_lb->loopback_skb_q);
#ifdef CONFIG_USNET_TIMER_LOOPBACK
		hrtimer_init(&usnet_lb->loopback_q_timer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
#endif

		mif_err("usnet_loopback start : num_desc:%d fore:%d done:%d rear:%d\n",
				q->q_info_ptr->num_desc, q->q_info_ptr->fore_ptr,
				q->done_ptr, q->q_info_ptr->rear_ptr);
		break;
	}

	default:
		mif_info("ERR! undefined cmd 0x%X\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static int usnet_chr_open(struct inode *inode, struct file *file)
{
	struct us_net *usnet_obj = container_of(file->private_data, struct us_net, ndev_miscdev);
	struct us_net_iod *usnet_iod = NULL;

	mif_info("+++\n");

	usnet_iod = (struct us_net_iod *)kzalloc(sizeof(struct us_net_iod), GFP_KERNEL);

	if (!usnet_iod) {
		mif_err("can't alloc usnet_iod\n");
		return -ENOMEM;
	}

	usnet_iod->usnet_obj = usnet_obj;
	file->private_data = (void *)usnet_iod;

	mif_err("usnet opened by %s(%d) ---\n", current->comm, current->pid);
	return 0;
}

static void usnet_close_loopback(struct us_net *usnet_obj, int client_idx)
{
	struct mem_link_device *mld = usnet_obj->mld;
	struct us_net_loopback_data *usnet_lb;
#ifdef FEATURE_LOOPBACK_RESTORE_Q_PTR
	struct us_net_loopback_pktproc_pointers *pktproc_pointers =
			&usnet_obj->pktproc_pointers;
	unsigned long flags = 0;
#endif

	if (!usnet_obj->loopback[client_idx].do_loopback)
		return;

	/* close loopback */
	usnet_lb = &usnet_obj->loopback[client_idx];

	usnet_obj->loopback[client_idx].do_loopback = false;
	skb_queue_purge(&usnet_lb->loopback_skb_q);
#ifdef CONFIG_USNET_TIMER_LOOPBACK
	hrtimer_cancel(&usnet_lb->loopback_q_timer);
#endif

#ifdef FEATURE_LOOPBACK_RESTORE_Q_PTR
	spin_lock_irqsave(&mld->pktproc.q[0]->lock, flags);
	*mld->pktproc.q[0]->fore_ptr = pktproc_pointers->fore_ptr;
	*mld->pktproc.q[0]->rear_ptr = pktproc_pointers->rear_ptr;
	mld->pktproc.q[0]->done_ptr = pktproc_pointers->done_ptr;
	spin_unlock_irqrestore(&mld->pktproc.q[0]->lock, flags);
#endif

	mif_info("usent_loopback after close : num_desc:%d fore:%d done:%d rear:%d\n",
			mld->pktproc.q[0]->q_info_ptr->num_desc,
			mld->pktproc.q[0]->q_info_ptr->fore_ptr,
			mld->pktproc.q[0]->done_ptr,
			mld->pktproc.q[0]->q_info_ptr->rear_ptr);
}

static int usnet_chr_close(struct inode *inode, struct file *filp)
{
	struct us_net_iod *usnet_iod = (struct us_net_iod *)filp->private_data;
	struct us_net_stack *usnet_client = usnet_iod->usnet_client;
	struct us_net *usnet_obj = usnet_iod->usnet_obj;
	int err;
	int client_idx;

	mif_info("+++\n");

	if (usnet_client) {
		client_idx = usnet_client->us_net_index;

		usnet_close_loopback(usnet_obj, client_idx);
		err = pktproc_destroy_usnet(usnet_client);

		usnet_obj->usnet_clients[client_idx] = NULL;
		atomic_dec(&usnet_obj->clients);
		kfree(usnet_client);

		mif_info("usnet client destoryed\n");
	}

	kfree(usnet_iod);

	mif_err("usnet closed by %s(%d) ---\n", current->comm, current->pid);
	return 0;
}

const struct file_operations usnet_io_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write_iter	= usnet_chr_write_iter,
	.poll		= usnet_chr_poll,
	.unlocked_ioctl	= usnet_chr_ioctl,
#ifdef CONFIG_COMPAT
	/* USNET: do we need to support this? */
#endif
	.open		= usnet_chr_open,
	.release	= usnet_chr_close,
};

static int usnet_pdn_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct us_net *usnet_obj = container_of(nb, struct us_net, inet_pdn_nb);
	struct us_net_stack *usnet_client;
	enum pdn_type evt = (enum pdn_type)action;
	struct io_device *iod = (struct io_device *)nb_data;
	int i;

	if (evt != PDN_DEFAULT)
		return 0;

	/* this backup is needed when usnet created */
	usnet_obj->inet_pdn_iod = iod;

	mif_info("inet pdn notified (%s)\n", iod->name);

	for (i = 0; i < US_NET_CLIENTS_MAX; i++) {
		usnet_client = usnet_obj->usnet_clients[i];

		if (!usnet_client)
			continue;

		/* USNET_TODO: need some lock here! */

		usnet_client->iod = iod;
	}
	return 0;
}

int init_usnet_io_device(struct device *dev, struct link_device *ld, struct modem_ctl *mc)
{
	int ret = 0;
	struct mem_link_device *mld = ld_to_mem_link_device(ld);

	usnet = (struct us_net *)kzalloc(sizeof(struct us_net), GFP_KERNEL);

	if (!usnet) {
		mif_err("ERR! can't allocate usnet\n");
		return -ENOMEM;
	}

	atomic_set(&usnet->clients, 0);
	usnet->mc  = mc;
	usnet->mld = mld;
	usnet->dev = dev;

	usnet->ndev_miscdev.minor = MISC_DYNAMIC_MINOR;
	usnet->ndev_miscdev.name = USNET_MISC_DEV;	// iod->name;
	usnet->ndev_miscdev.nodename = USNET_MISC_DEV;	// /dev/usnet0
	usnet->ndev_miscdev.fops = &usnet_io_fops;

	ret = misc_register(&usnet->ndev_miscdev);

	if (ret) {
		mif_info("ERR! usnet misc_register failed\n");

		kfree(usnet);
		usnet = NULL;
	}

	usnet->inet_pdn_nb.notifier_call = usnet_pdn_notifier;
	register_pdn_event_notifier(&usnet->inet_pdn_nb);

	ld->usnet_obj = usnet;

	return ret;
}

