// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <soc/samsung/exynos-smc.h>

#include <linux/skbuff.h>
#if IS_ENABLED(CONFIG_OF_RESERVED_MEM)
#include <linux/of_reserved_mem.h>
#endif

#include <soc/samsung/shm_ipc.h>

#include "include/gnss.h"
#include "gnss_link_device_shmem.h"

#include "gnss_prj.h"
#include "gnss_utils.h"

void gnss_write_reg(struct shmem_link_device *shmd,
		enum gnss_reg_type reg, u32 value)
{
	struct gnss_shared_reg *gnss_reg = shmd->reg[reg];
	if (gnss_reg) {
		switch (gnss_reg->device) {
		case GNSS_IPC_MBOX:
			gnss_mbox_set_sr(shmd->mbx->id, gnss_reg->value.index, value);
			break;
		case GNSS_IPC_SHMEM:
			iowrite32(value, gnss_reg->value.addr);
			break;
		default:
			gif_err("Don't know where to write register! (%d)\n",
					gnss_reg->device);
		}
	} else {
		gif_err("Couldn't find the register node.\n");
	}
}

u32 gnss_read_reg(struct shmem_link_device *shmd, enum gnss_reg_type reg)
{
	struct gnss_shared_reg *gnss_reg = shmd->reg[reg];
	u32 ret = 0;

	if (gnss_reg) {
		switch (gnss_reg->device) {
		case GNSS_IPC_MBOX:
			ret = gnss_mbox_get_sr(shmd->mbx->id, gnss_reg->value.index);
			break;
		case GNSS_IPC_SHMEM:
			ret = ioread32(gnss_reg->value.addr);
			break;
		default:
			gif_err("Don't know where to read register from! (%d)\n",
					gnss_reg->device);
		}
	} else {
		gif_err("Couldn't find the register node.\n");
	}

	return ret;
}

/**
 * recv_int2ap
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of the GNSS-to-AP interrupt register.
 */
static inline u16 recv_int2ap(struct shmem_link_device *shmd)
{
	return (u16)gnss_mbox_get_sr(shmd->mbx->id, shmd->mbx->irq_ipc_msg);
}

/**
 * send_int2cp
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mask: value to be written to the AP-to-GNSS interrupt register
 */
static inline void send_int2gnss(struct shmem_link_device *shmd, u16 mask)
{
	gnss_write_reg(shmd, GNSS_REG_TX_IPC_MSG, mask);
	gnss_mbox_set_interrupt(shmd->mbx->id, shmd->mbx->int_ipc_msg);
}

/**
 * get_shmem_status
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dir: direction of communication (TX or RX)
 * @mst: pointer to an instance of mem_status structure
 *
 * Takes a snapshot of the current status of a SHMEM.
 */
static void get_shmem_status(struct shmem_link_device *shmd,
		enum direction dir, struct mem_status *mst)
{
	mst->dir = dir;
	mst->head[TX] = get_txq_head(shmd);
	mst->tail[TX] = get_txq_tail(shmd);
	mst->head[RX] = get_rxq_head(shmd);
	mst->tail[RX] = get_rxq_tail(shmd);
	mst->int2ap = recv_int2ap(shmd);
	mst->int2gnss = read_int2gnss(shmd);

	gif_debug("----- %s -----\n", __func__);
	gif_debug("%s: mst->dir      = %d\n", __func__, mst->dir);
	gif_debug("%s: mst->head[TX] = %d\n", __func__, mst->head[TX]);
	gif_debug("%s: mst->tail[TX] = %d\n", __func__, mst->tail[TX]);
	gif_debug("%s: mst->head[RX] = %d\n", __func__, mst->head[RX]);
	gif_debug("%s: mst->tail[RX] = %d\n", __func__, mst->tail[RX]);
	gif_debug("%s: mst->int2ap   = %d\n", __func__, mst->int2ap);
	gif_debug("%s: mst->int2gnss = %d\n", __func__, mst->int2gnss);
	gif_debug("----- %s -----\n", __func__);
}

static inline void update_rxq_tail_status(struct shmem_link_device *shmd,
		struct mem_status *mst)
{
	mst->tail[RX] = get_rxq_tail(shmd);
}

/**
 * ipc_rx_work
 * @ws: pointer to an instance of work_struct structure
 *
 * Invokes the recv method in the io_device instance to perform receiving IPC
 * messages from each skb.
 */
static void msg_rx_work(struct work_struct *ws)
{
	struct shmem_link_device *shmd;
	struct link_device *ld;
	struct io_device *iod;
	struct sk_buff *skb;

	shmd = container_of(ws, struct shmem_link_device, msg_rx_dwork.work);
	ld = &shmd->ld;

	iod = ld->iod;
	while (1) {
		skb = skb_dequeue(ld->skb_rxq);
		if (!skb)
			break;
		if (iod->recv_skb_single)
			iod->recv_skb_single(iod, ld, skb);
		else
			gif_err("ERR! iod->recv_skb_single undefined!\n");
	}
}

/**
 * rx_ipc_frames
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * Returns
 *   ret < 0  : error
 *   ret == 0 : ILLEGAL status
 *   ret > 0  : valid data
 *
 * Must be invoked only when there is data in the corresponding RXQ.
 *
 * Requires a recv_skb method in the io_device instance, so this function must
 * be used for only EXYNOS.
 */
static int rx_ipc_frames(struct shmem_link_device *shmd,
			struct circ_status *circ)
{
	struct link_device *ld = &shmd->ld;
	struct io_device *iod;
	struct sk_buff_head *rxq = ld->skb_rxq;
	struct sk_buff *skb;
	/**
	 * variables for the status of the circular queue
	 */
	u8 *src;
	u8 hdr[EXYNOS_HEADER_SIZE];
	/**
	 * variables for RX processing
	 */
	int qsize;	/* size of the queue			*/
	int rcvd;	/* size of data in the RXQ or error	*/
	int rest;	/* size of the rest data		*/
	int out;	/* index to the start of current frame	*/
	int tot;	/* total length including padding data	*/

	src = circ->buff;
	qsize = circ->qsize;
	out = circ->out;
	rcvd = circ->size;

	rest = circ->size;
	tot = 0;

	while (rest > 0) {
		u8 ch;

		/* Copy the header in the frame to the header buffer */
		circ_read(hdr, src, qsize, out, EXYNOS_HEADER_SIZE);

		/* Check the config field in the header */
		if (unlikely(!exynos_start_valid(hdr))) {
			gif_err("%s: ERR! %s INVALID config 0x%02X (rcvd %d, rest %d)\n",
				ld->name, "FMT", hdr[0],
				rcvd, rest);
			goto bad_msg;
		}

		/* Verify the total length of the frame (data + padding) */
		tot = exynos_get_total_len(hdr);
		if (unlikely(tot > rest)) {
			gif_err("%s: ERR! %s tot %d > rest %d (rcvd %d)\n",
				ld->name, "FMT", tot, rest, rcvd);
			goto bad_msg;
		}

		/* Allocate an skb */
		skb = dev_alloc_skb(tot);
		if (!skb) {
			gif_err("%s: ERR! %s dev_alloc_skb(%d) fail\n",
				ld->name, "FMT", tot);
			goto no_mem;
		}

		/* Set the attribute of the skb as "single frame" */
		skbpriv(skb)->single_frame = true;

		/* Read the frame from the RXQ */
		circ_read(skb_put(skb, tot), src, qsize, out, tot);

		/* Store the skb to the corresponding skb_rxq */
		skb_queue_tail(rxq, skb);

		ch = exynos_get_ch(skb->data);
		iod = ld->iod;
		if (!iod) {
			gif_err("%s: ERR! no IPC_BOOT iod\n", ld->name);
			break;
		}

		skbpriv(skb)->lnk_hdr = iod->link_header;
		skbpriv(skb)->exynos_ch = ch;

		/* Calculate new out value */
		rest -= tot;
		out += tot;
		if (unlikely(out >= qsize))
			out -= qsize;
	}

	/* Update tail (out) pointer to empty out the RXQ */
	set_rxq_tail(shmd, circ->in);
	return rcvd;

no_mem:
	/* Update tail (out) pointer to the frame to be read in the future */
	set_rxq_tail(shmd, out);
	rcvd -= rest;
	return rcvd;

bad_msg:
	return -EBADMSG;
}

/**
 * msg_handler: receives IPC messages from every RXQ
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * 1) Receives all IPC message frames currently in every IPC RXQ.
 * 2) Sends RES_ACK responses if there are REQ_ACK requests from a GNSS.
 * 3) Completes all threads waiting for the corresponding RES_ACK from a GNSS if
 *    there is any RES_ACK response.
 */
static void msg_handler(struct shmem_link_device *shmd, struct mem_status *mst)
{
	struct link_device *ld = &shmd->ld;
	struct circ_status circ;
	int ret = 0;

	/* Skip RX processing if there is no data in the RXQ */
	if (mst->head[RX] == mst->tail[RX]) {
		/* Release wakelock */
		/* Write 0x0 to mbox register 6 */
		/* done_req_ack(shmd); */
		return;

	}

	/* Get the size of data in the RXQ */
	ret = get_rxq_rcvd(shmd, mst, &circ);
	if (unlikely(ret < 0)) {
		gif_err("%s: ERR! get_rxq_rcvd fail (err %d)\n",
			ld->name, ret);
		return;
	}

	/* Read data in the RXQ */
	ret = rx_ipc_frames(shmd, &circ);
	if (unlikely(ret < 0)) {
		gif_err_limited("rx_ipc_frames err:%d\n", ret);
		return;
	}
}

/**
 * ipc_rx_task: processes a SHMEM command or receives IPC messages
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mst: pointer to an instance of mem_status structure
 *
 * Invokes cmd_handler for commands or msg_handler for IPC messages.
 */
static void ipc_rx_task(unsigned long data)
{
	struct shmem_link_device *shmd = (struct shmem_link_device *)data;

	while (1) {
		struct mem_status *mst;

		mst = gnss_msq_get_data_slot(&shmd->rx_msq);
		if (!mst)
			break;
		memset(mst, 0, sizeof(struct mem_status));

		get_shmem_status(shmd, RX, mst);

		msg_handler(shmd, mst);

		queue_delayed_work(system_wq, &shmd->msg_rx_dwork, 0);
	}
}

/**
 * shmem_irq_handler: interrupt handler for a MCU_IPC interrupt
 * @data: pointer to a data
 *
 * 1) Reads the interrupt value
 * 2) Performs interrupt handling
 *
 * Flow for normal interrupt handling:
 *   shmem_irq_handler -> udl_handler
 *   shmem_irq_handler -> ipc_rx_task -> msg_handler -> rx_ipc_frames ->  ...
 */
static irqreturn_t shmem_irq_msg_handler(int irq, void *data)
{
	struct shmem_link_device *shmd = (struct shmem_link_device *)data;

	gnss_msq_get_free_slot(&shmd->rx_msq);

	shmd->rx_int_count++;

	tasklet_hi_schedule(&shmd->rx_tsk);

	return IRQ_HANDLED;
}

/**
 * write_ipc_to_txq
 * @shmd: pointer to an instance of shmem_link_device structure
 * @circ: pointer to an instance of circ_status structure
 * @skb: pointer to an instance of sk_buff structure
 *
 * Must be invoked only when there is enough space in the TXQ.
 */
static void write_ipc_to_txq(struct shmem_link_device *shmd,
			struct circ_status *circ, struct sk_buff *skb)
{
	u32 qsize = circ->qsize;
	u32 in = circ->in;
	u8 *buff = circ->buff;
	u8 *src = skb->data;
	u32 len = skb->len;
	u32 head_prev;
	u32 tail_prev;

#if defined(DEBUG_GNSS_IPC_PKT)
	/* Print send data to GNSS */
	gnss_log_ipc_pkt(skb, TX);
#endif

	/* Write data to the TXQ */
	circ_write(buff, src, qsize, in, len);

	/* Update new head (in) pointer */
	head_prev = get_txq_head(shmd);
	tail_prev = get_txq_tail(shmd);
	set_txq_head(shmd, circ_new_pointer(qsize, in, len));
	gif_debug("TXQ head: %d->%d TXQ tail: %d->%d\n",
			head_prev, get_txq_head(shmd),
			tail_prev, get_txq_tail(shmd));
}

/**
 * xmit_ipc_msg
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Tries to transmit IPC messages in the skb_txq of @dev as many as possible.
 *
 * Returns total length of IPC messages transmit or an error code.
 */
static int xmit_ipc_msg(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	struct sk_buff_head *txq = ld->skb_txq;
	struct sk_buff *skb;
	unsigned long flags;
	struct circ_status circ;
	int space;
	int copied = 0;
	bool chk_nospc = false;

	/* Acquire the spin lock for a TXQ */
	spin_lock_irqsave(&shmd->tx_lock, flags);

	while (1) {
		/* Get the size of free space in the TXQ */
		space = get_txq_space(shmd, &circ);
		if (unlikely(space < 0)) {
			/* Empty out the TXQ */
			reset_txq_circ(shmd);
			copied = -EIO;
			break;
		}

		skb = skb_dequeue(txq);
		if (unlikely(!skb))
			break;

		/*
		 * CAUTION : Uplink size is limited to 16KB and
		 * this limitation is used ONLY in North America Prj.
		 * Check the free space size,
		 * - FMT : comparing with skb->len
		 * - RAW : check used buffer size
		 */
		chk_nospc = (space < skb->len) ? true : false;
		if (unlikely(chk_nospc)) {
			/* Set res_required flag */
			atomic_set(&shmd->res_required, skb->len);

			/* Take the skb back to the skb_txq */
			skb_queue_head(txq, skb);

			gif_err("%s: <by %ps> NOSPC in %s_TXQ {qsize:%u in:%u out:%u} free:%u < len:%u\n",
				ld->name, CALLER, "FMT",
				circ.qsize, circ.in, circ.out, space, skb->len);
			copied = -ENOSPC;
			break;
		}

		/* TX only when there is enough space in the TXQ */
		write_ipc_to_txq(shmd, &circ, skb);
		copied += skb->len;
		dev_kfree_skb_any(skb);
	}

	/* Release the spin lock */
	spin_unlock_irqrestore(&shmd->tx_lock, flags);

	return copied;
}

/**
 * fmt_tx_work: performs TX for FMT IPC device under SHMEM flow control
 * @ws: pointer to an instance of the work_struct structure
 *
 * 1) Starts waiting for RES_ACK of FMT IPC device.
 * 2) Returns immediately if the wait is interrupted.
 * 3) Restarts SHMEM flow control if there is a timeout from the wait.
 * 4) Otherwise, it performs processing RES_ACK for FMT IPC device.
 */
static void fmt_tx_work(struct work_struct *ws)
{
	struct link_device *ld;
	struct shmem_link_device *shmd;
	struct circ_status circ;
	int space;
	int space_needed;

	ld = container_of(ws, struct link_device, fmt_tx_dwork.work);
	shmd = to_shmem_link_device(ld);

	space = get_txq_space(shmd, &circ);
	space_needed = atomic_read(&shmd->res_required);

	if (unlikely(space_needed < space))
		queue_delayed_work(ld->tx_wq, ld->tx_dwork,
				msecs_to_jiffies(1));
	else
		atomic_set(&shmd->res_required, 0);
}

/**
 * shmem_send_ipc
 * @shmd: pointer to an instance of shmem_link_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Tries to transmit IPC messages in the skb_txq with xmit_ipc_msg().
 * 2) Sends an interrupt to GNSS if there is no error from xmit_ipc_msg().
 * 3) Starts SHMEM flow control if xmit_ipc_msg() returns -ENOSPC.
 */
static int shmem_send_ipc(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	int ret;

	if (atomic_read(&shmd->res_required) > 0) {
		gif_err("%s: %s_TXQ is full\n", ld->name, "FMT");
		return 0;
	}

	ret = xmit_ipc_msg(shmd);
	if (likely(ret > 0)) {
		gif_debug("Setting interrupt\n");
		send_int2gnss(shmd, 0x82);
		goto exit;
	}

	/* If there was no TX, just exit */
	if (ret == 0)
		goto exit;

	/* At this point, ret < 0 */
	if (ret == -ENOSPC || ret == -EBUSY) {
		/*----------------------------------------------------*/
		/* shmd->res_required was set in xmit_ipc_msg(). */
		/*----------------------------------------------------*/

		queue_delayed_work(ld->tx_wq, ld->tx_dwork,
				   msecs_to_jiffies(1));
	}

exit:
	return ret;
}

/**
 * shmem_try_send_ipc
 * @shmd: pointer to an instance of shmem_link_device structure
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * 1) Enqueues an skb to the skb_txq for @dev in the link device instance.
 * 2) Tries to transmit IPC messages with shmem_send_ipc().
 */
static void shmem_try_send_ipc(struct shmem_link_device *shmd,
			struct io_device *iod, struct sk_buff *skb)
{
	struct link_device *ld = &shmd->ld;
	struct sk_buff_head *txq = ld->skb_txq;
	int ret;

	if (unlikely(txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		gif_err("%s: %s txq->qlen %d >= %d\n", ld->name,
			"FMT", txq->qlen, MAX_SKB_TXQ_DEPTH);
		dev_kfree_skb_any(skb);
		return;
	}

	skb_queue_tail(txq, skb);

	ret = shmem_send_ipc(shmd);
	if (ret < 0) {
		gif_err("%s->%s: ERR! shmem_send_ipc fail (err %d)\n",
			iod->name, ld->name, ret);
	}
}

/**
 * shmem_send
 * @ld: pointer to an instance of the link_device structure
 * @iod: pointer to an instance of the io_device structure
 * @skb: pointer to an skb that will be transmitted
 *
 * Returns the length of data transmitted or an error code.
 *
 * Normal call flow for an IPC message:
 *   shmem_try_send_ipc -> shmem_send_ipc -> xmit_ipc_msg -> write_ipc_to_txq
 *
 * Call flow on congestion in a IPC TXQ:
 *   shmem_try_send_ipc -> shmem_send_ipc -> xmit_ipc_msg ,,, queue_delayed_work
 *   => xxx_tx_work -> wait_for_res_ack
 *   => msg_handler
 *   => process_res_ack -> xmit_ipc_msg (,,, queue_delayed_work ...)
 */
static int shmem_send(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	int len = skb->len;

	shmem_try_send_ipc(shmd, iod, skb);

	return len;
}

static void shmem_remap_ipc_region(struct shmem_link_device *shmd)
{
	struct shmem_ipc_device *dev;
	struct gnss_shared_reg *shreg;
	u32 *reg_base;
	int i;

	/* FMT */
	dev = &shmd->ipc_map.dev;

	dev->rxq.buff = (u8 __iomem *)(shmd->ipc_rx_mem.vaddr);
	dev->rxq.size = shmd->ipc_rx_mem.size;

	dev->txq.buff = (u8 __iomem *)(shmd->ipc_tx_mem.vaddr);
	dev->txq.size = shmd->ipc_tx_mem.size;

	reg_base = (u32 *)(shmd->ipc_reg_mem.vaddr);

	gif_info("RX region : %x @ %pK\n", dev->rxq.size, dev->rxq.buff);
	gif_info("TX region : %x @ %pK\n", dev->txq.size, dev->txq.buff);

	for (i = 0; i < GNSS_REG_COUNT; i++) {
		shreg = shmd->reg[i];
		if (shreg && (shreg->device == GNSS_IPC_SHMEM)) {
			shreg->value.addr = reg_base + shreg->value.index;
			gif_info("Reg %s -> %pK\n", shreg->name, shreg->value.addr);
		}
	}
}

static int shmem_init_ipc_map(struct shmem_link_device *shmd)
{
	shmem_remap_ipc_region(shmd);

	memset(shmd->ipc_mem.vaddr, 0x0, shmd->ipc_mem.size);

	shmd->dev = &shmd->ipc_map.dev;

	return 0;
}

static void shmem_reset_buffers(struct link_device *ld)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	purge_txq(ld);
	purge_rxq(ld);
	clear_shmem_map(shmd);
}

static int shmem_copy_reserved_to_user(struct link_device *ld, u32 offset,
			void __user *user_dst, u32 size)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	int err = 0;

	if ((size > shmd->res_mem.size) ||
		(offset > (shmd->res_mem.size - size))) {
		gif_err("Unable to read %d bytes @ 0x%p+0x%x\n", size,
				shmd->res_mem.vaddr, offset);
		err = -EFAULT;
		goto shmem_copy_to_fault;
	}

	gif_debug("base addr = 0x%pK\n", shmd->res_mem.vaddr);
	err = copy_to_user(user_dst, (void *)shmd->res_mem.vaddr + offset,
				size);
	if (err) {
		gif_err("copy_to_user fail\n");
		err = -EFAULT;
		goto shmem_copy_to_fault;
	}

shmem_copy_to_fault:
	return err;
}

static int shmem_copy_reserved_from_user(struct link_device *ld, u32 offset,
			void __user *user_src, u32 size)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	int err = 0;

	if ((size > shmd->res_mem.size) ||
		(offset > (shmd->res_mem.size - size))) {
		gif_err("Unable to load %d bytes @ 0x%p+0x%x\n", size,
				shmd->res_mem.vaddr, offset);
		err = -EFAULT;
		goto shmem_copy_from_fault;
	}

	gif_info("base addr = 0x%pK\n", shmd->res_mem.vaddr);
	err = copy_from_user((void *)shmd->res_mem.vaddr + offset,
				user_src, size);
	if (err) {
		gif_err("copy_from_user fail\n");
		err = -EFAULT;
		goto shmem_copy_from_fault;
	}

shmem_copy_from_fault:
	return err;
}

static int shmem_dump_fault_mem_to_user(struct link_device *ld,
			void __user *user_dst, u32 size)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	int err = 0;

	if (size > shmd->fault_mem.size) {
		gif_err("Unable to dump %d bytes (max %d bytes)\n", size,
				shmd->fault_mem.size);
		err = -EFAULT;
		goto shmem_dump_mem_to_fault;
	}

	gif_info("fault addr = 0x%pK\n", shmd->fault_mem.vaddr);
	err = copy_to_user(user_dst, shmd->fault_mem.vaddr, size);

	if (err) {
		gif_err("copy_to_user fail\n");
		err = -EFAULT;
		goto shmem_dump_mem_to_fault;
	}

shmem_dump_mem_to_fault:
	return err;
}

static int shmem_dump_fault_mbx_to_user(struct link_device *ld,
			void __user *user_dst, u32 size)
{
	struct gnss_pdata *pdata;
	struct gnss_mbox *mbx;
	u32 *dump_info;
	unsigned int max_size;
	int reg_cnt;
	int err = 0;
	int i;

	pdata = ld->pdata;
	mbx = pdata->mbx;

	reg_cnt = pdata->fault_info.size;
	max_size = reg_cnt * sizeof(u32);

	if (size > max_size) {
		gif_err("Unable to dump %d bytes (max %d bytes)\n", size,
				max_size);
		err = -EFAULT;
		goto shmem_dump_mbx_to_fault;
	}

	dump_info = kzalloc(max_size, GFP_KERNEL);
	if (!dump_info) {
		gif_err("Could not allocate fault info memory\n");
		err = -ENOMEM;
		goto shmem_dump_mbx_to_fault;
	}

	for (i = 0; i < reg_cnt; i++)
		dump_info[i] = gnss_mbox_get_sr(mbx->id,
				pdata->fault_info.value.index + i);

	err = copy_to_user(user_dst, dump_info, size);
	kfree(dump_info);
	if (err) {
		gif_err("copy_to_user fail\n");
		err = -EFAULT;
		goto shmem_dump_mbx_to_fault;
	}

shmem_dump_mbx_to_fault:
	return err;
}

static ssize_t map_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gnss_pdata *pdata = (struct gnss_pdata *)dev->platform_data;

	return sprintf(buf, "Reserved Region: 0x00000000 0x%08X\n"
			"Fault Region: 0x%08X 0x%08X\n"
			"IPC Region: 0x%08X 0x%08X\n",
			pdata->shmem_size,
			pdata->fault_info.value.index, pdata->fault_info.size,
			pdata->ipc_offset, pdata->ipc_size);
}

static ssize_t shm_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gnss_pdata *pdata = (struct gnss_pdata *)dev->platform_data;
	struct shmem_link_device *shmd = pdata->shmd;

	return sprintf(buf, "TX head: 0x%08X\nTX tail: 0x%08X\n"
			"RX head: 0x%08X\nRX tail: 0x%08X\n\n"
			"int2gnss: 0x%08X\nint2ap: 0x%08X\n"
			"rx_int_count: 0x%08X\n",
			get_txq_head(shmd), get_txq_tail(shmd),
			get_rxq_head(shmd), get_rxq_tail(shmd),
			read_int2gnss(shmd), recv_int2ap(shmd), shmd->rx_int_count);
}

static DEVICE_ATTR_RO(map_info);
static DEVICE_ATTR_RO(shm_status);

static struct attribute *shmem_attrs[] = {
	&dev_attr_map_info.attr,
	&dev_attr_shm_status.attr,
	NULL,
};

static const struct attribute_group shmem_group = {
	.attrs = shmem_attrs,
	.name = "shmem",
};

static void __iomem *get_nc_region(phys_addr_t base, size_t size)
{
	int i;
	unsigned int num_pages = (unsigned int)(size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages;
	void *v_addr;
	unsigned int alloc_size;

	if (!base)
		return NULL;

	if (size > (num_pages << PAGE_SHIFT))
		num_pages++;

	alloc_size = sizeof(struct page *) * num_pages;
	pages = kmalloc(alloc_size, GFP_ATOMIC);
	if (!pages) {
		gif_err("pages allocation fail!\n");
		return NULL;
	}

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(base);
		base += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	if (v_addr == NULL)
		gif_err("Failed to vmap pages\n");

	kfree(pages);

	return (void __iomem *)v_addr;
}

struct link_device *create_link_device_shmem(struct platform_device *pdev)
{
	struct shmem_link_device *shmd = NULL;
	struct link_device *ld = NULL;
	struct gnss_pdata *pdata = NULL;
	struct device *dev = &pdev->dev;
	int err = 0;

	gif_info("+++\n");

	/* Get the gnss (platform) data */
	pdata = (struct gnss_pdata *)dev->platform_data;
	if (!pdata) {
		gif_err("ERR! gnss == NULL\n");
		return NULL;
	}
	gif_info("%s: %s\n", "SHMEM", pdata->name);

	if (!pdata->mbx) {
		gif_err("%s: ERR! %s->mbx == NULL\n",
			"SHMEM", pdata->name);
		return NULL;
	}

	/* Alloc an instance of shmem_link_device structure */
	shmd = devm_kzalloc(dev, sizeof(struct shmem_link_device), GFP_KERNEL);
	if (!shmd) {
		gif_err("%s: ERR! shmd kzalloc fail\n", "SHMEM");
		goto error;
	}
	ld = &shmd->ld;
	shmd->mbx = pdata->mbx;

	/* Retrieve gnss data and SHMEM control data from the gnss data */
	ld->pdata = pdata;
	ld->timeout_cnt = 0;
	ld->name = "GNSS_SHDMEM";

	/* Assign reserved memory methods */
	ld->copy_reserved_from_user = shmem_copy_reserved_from_user;
	ld->copy_reserved_to_user = shmem_copy_reserved_to_user;

	ld->reset_buffers = shmem_reset_buffers;

	/* Set attributes as a link device */
	ld->send = shmem_send;

	skb_queue_head_init(&ld->sk_fmt_tx_q);
	ld->skb_txq = &ld->sk_fmt_tx_q;

	skb_queue_head_init(&ld->sk_fmt_rx_q);
	ld->skb_rxq = &ld->sk_fmt_rx_q;

	/* Initialize GNSS Reserved mem */
	shmd->res_mem.size = pdata->shmem_size;
	shmd->res_mem.paddr = pdata->shmem_base;
	shmd->res_mem.vaddr = get_nc_region(shmd->res_mem.paddr, shmd->res_mem.size);
	if (!shmd->res_mem.vaddr) {
		gif_err("%s: ERR! gnss_reserved_region fail\n", ld->name);
		goto error;
	}
	gif_info("%s:Reserved Region phys_addr:0x%llX virt_addr:%pK size:%d\n",
		 ld->name, shmd->res_mem.paddr,
		 shmd->res_mem.vaddr, shmd->res_mem.size);

	/* Initialize GNSS fault info area */
	if (pdata->fault_info.device == GNSS_IPC_SHMEM) {
		ld->dump_fault_to_user = shmem_dump_fault_mem_to_user;
		shmd->fault_mem.size = pdata->fault_info.size;
		shmd->fault_mem.paddr = pdata->shmem_base + pdata->fault_info.value.index;
		shmd->fault_mem.vaddr = get_nc_region(
				shmd->fault_mem.paddr, shmd->fault_mem.size);
		if (!shmd->fault_mem.vaddr) {
			gif_err("%s: ERR! fault get_nc_region fail\n",
					ld->name);
			goto error;
		}
		gif_info("%s:Fault Region phys_addr:0x%llX virt_addr:%pK size:%d\n",
			 ld->name, shmd->fault_mem.paddr,
			 shmd->fault_mem.vaddr, shmd->fault_mem.size);
	} else {
		ld->dump_fault_to_user = shmem_dump_fault_mbx_to_user;
		shmd->fault_mem.size = pdata->fault_info.size;
	}

	/* Initialize GNSS IPC region */
	shmd->reg = pdata->reg;

	shmd->ipc_mem.size = pdata->ipc_size;
	shmd->ipc_mem.paddr = pdata->shmem_base + pdata->ipc_offset;
	shmd->ipc_mem.vaddr = get_nc_region(shmd->ipc_mem.paddr,
			shmd->ipc_mem.size);
	if (!shmd->ipc_mem.vaddr) {
		gif_err("%s: ERR! ipc get_nc_region fail\n",
				ld->name);
		goto error;
	}

	gif_info("%s:IPC Region phys_addr:0x%llX virt_addr:%pK size:%d\n",
		 ld->name, shmd->ipc_mem.paddr,
		 shmd->ipc_mem.vaddr, shmd->ipc_mem.size);

	shmd->ipc_rx_mem.size = pdata->ipc_rx_size;
	shmd->ipc_rx_mem.paddr = shmd->ipc_mem.paddr + pdata->ipc_rx_offset;
	shmd->ipc_rx_mem.vaddr = shmd->ipc_mem.vaddr + pdata->ipc_rx_offset;

	gif_info("%s:IPC RX Region phys_addr:0x%llX virt_addr:%pK size:%d\n",
		 ld->name, shmd->ipc_rx_mem.paddr,
		 shmd->ipc_rx_mem.vaddr, shmd->ipc_rx_mem.size);

	shmd->ipc_tx_mem.size = pdata->ipc_tx_size;
	shmd->ipc_tx_mem.paddr = shmd->ipc_mem.paddr + pdata->ipc_tx_offset;
	shmd->ipc_tx_mem.vaddr = shmd->ipc_mem.vaddr + pdata->ipc_tx_offset;

	gif_info("%s:IPC TX Region phys_addr:0x%llX virt_addr:%pK size:%d\n",
		 ld->name, shmd->ipc_tx_mem.paddr,
		 shmd->ipc_tx_mem.vaddr, shmd->ipc_tx_mem.size);

	shmd->ipc_reg_mem.size = pdata->ipc_reg_size;
	shmd->ipc_reg_mem.paddr = shmd->ipc_mem.paddr + pdata->ipc_reg_offset;
	shmd->ipc_reg_mem.vaddr = shmd->ipc_mem.vaddr + pdata->ipc_reg_offset;

	gif_info("%s:IPC REG Region phys_addr:0x%llX virt_addr:%pK size:%d\n",
		 ld->name, shmd->ipc_reg_mem.paddr,
		 shmd->ipc_reg_mem.vaddr, shmd->ipc_reg_mem.size);

	/* Initialize SHMEM maps (physical map -> logical map) */
	err = shmem_init_ipc_map(shmd);
	if (err < 0) {
		gif_err("%s: ERR! shmem_init_ipc_map fail (err %d)\n",
			ld->name, err);
		goto error;
	}

	tasklet_init(&shmd->rx_tsk, ipc_rx_task, (unsigned long)shmd);
	INIT_DELAYED_WORK(&shmd->msg_rx_dwork, msg_rx_work);

	spin_lock_init(&shmd->tx_lock);

	ld->tx_wq = create_singlethread_workqueue("shmem_tx_wq");
	if (!ld->tx_wq) {
		gif_err("%s: ERR! fail to create tx_wq\n", ld->name);
		goto error;
	}

	INIT_DELAYED_WORK(&ld->fmt_tx_dwork, fmt_tx_work);
	ld->tx_dwork = &ld->fmt_tx_dwork;

	spin_lock_init(&shmd->tx_msq.lock);
	spin_lock_init(&shmd->rx_msq.lock);

	/* Register interrupt handlers */
	err = gnss_mbox_register_irq(shmd->mbx->id, shmd->mbx->irq_ipc_msg,
			       shmem_irq_msg_handler, shmd);
	if (err) {
		gif_err("%s: ERR! gnss_mbox_request_irq fail (err %d)\n",
			ld->name, err);
		goto error;
	}

	/* Link link device to gnss_pdata */
	pdata->shmd = shmd;

	if (sysfs_create_group(&pdev->dev.kobj, &shmem_group))
		gif_err("failed to create sysfs node\n");

	gif_info("---\n");
	return ld;

error:
	gif_err("xxx\n");
	devm_kfree(dev, shmd);
	return NULL;
}
