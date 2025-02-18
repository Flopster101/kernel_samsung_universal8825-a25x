// SPDX-License-Identifier: GPL-2.0-or-later

#if !IS_ENABLED(CONFIG_KUNIT)
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/fdtable.h>

#include <net/protocol.h>
#include <net/dst.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/skb_tracer.h>

#include <linux/uaccess.h>
#include <trace/events/skb.h>
#include <linux/highmem.h>
#include <linux/capability.h>
#include <linux/user_namespace.h>

/* for KUNIT test */
#if IS_ENABLED(CONFIG_KUNIT)
#include <kunit/test.h>
#include <kunit/mock.h>

extern struct kunit *g_test;

/* kunit define wrapper */
#define STATIC
#define MOCKABLE(func)        	__weak __real__##func

/* define mockable functions */
DEFINE_REDIRECT_MOCKABLE(skb_tracer_gen_mask, RETURNS(u64), PARAMS(struct sock *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(print_sock_info, PARAMS(struct sock *, struct task_struct *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(skb_tracer_print_skb_queue, PARAMS(struct sk_buff_head *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(skb_tracer_print_skb_rb_tree, PARAMS(struct rb_root *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(skb_tracer_print_skb, PARAMS(struct skb_tracer *, struct sk_buff *));

#else
#define STATIC			static
#define MOCKABLE(func)		func

#endif

/* for test tracing target */
#define TRACER_MASK_MAX_BIT	64
#define TASK_NAME_IMS		"com.sec.imsservice"
#define TASK_NAME_TEST		"NetworkService"

static spinlock_t skb_tracer_sock_mask_lock;
STATIC u64 skb_tracer_sock_mask = 0xFFFFFFFFFFFFFFFF;

void skb_tracer_func_trace(const struct sock *sk,
		struct sk_buff *skb, enum skb_tracer_location location)
{
	struct skb_tracer *tracer;

	if (!sk || sk->sk_tracer_mask == 0)
		return;

	tracer = skb_ext_find(skb, SKB_TRACER);
	if (tracer)
		tracer->skb_mask |= location;

	//pr_info("%s: sk: %p, skb: %p, mask: 0x%x\n", location, sk, skb, tracer->skb_mask);

}
EXPORT_SYMBOL(skb_tracer_func_trace);

void skb_tracer_mask(struct sk_buff *skb, u64 mask)
{
	if (mask) {
		/* For now, assume that this mask is included in the sock */
		struct skb_tracer *tracer = skb_ext_find(skb, SKB_TRACER);
		if (!tracer)
			tracer = skb_ext_add(skb, SKB_TRACER);
		if (tracer) {
			tracer->queue_mask |= mask;
		}
	}
}
EXPORT_SYMBOL(skb_tracer_mask);

void skb_tracer_unmask(struct sk_buff *skb, u64 mask)
{
	if (mask) {
		/* For now, assume that this mask is included in the sock */
		struct skb_tracer *tracer = skb_ext_find(skb, SKB_TRACER);
		if (tracer) {
			tracer->queue_mask &= ~(mask);
		}
	}
}
EXPORT_SYMBOL(skb_tracer_unmask);

/* mask a bit on skb when transferring */
void skb_tracer_mask_on_skb(struct sock *sk, struct sk_buff *skb)
{
	struct skb_tracer *tracer;
	if (!sk->sk_tracer_mask)
		return;

	tracer = skb_ext_add(skb, SKB_TRACER);
	tracer->sock_mask = sk->sk_tracer_mask;
	tracer->skb_mask = 0;

	spin_lock_init(&tracer->lock);
}
EXPORT_SYMBOL(skb_tracer_mask_on_skb);

void skb_tracer_return_mask(struct sock *sk)
{
	unsigned long flags;

	if (sk->sk_tracer_mask == 0)
		return;

	pr_info("return: sk=0x%llx(ref=%d) by 0x%llx", (u64)sk,
			refcount_read(&sk->sk_refcnt), sk->sk_tracer_mask);

	spin_lock_irqsave(&skb_tracer_sock_mask_lock, flags);
	skb_tracer_sock_mask |= sk->sk_tracer_mask;
	spin_unlock_irqrestore(&skb_tracer_sock_mask_lock, flags);

	sk->sk_tracer_mask = 0;
}
EXPORT_SYMBOL(skb_tracer_return_mask);

STATIC void MOCKABLE(skb_tracer_print_skb)(struct skb_tracer *tracer, struct sk_buff *skb)
{
	pr_info("mask=(0x%llx, 0x%llx), len=%d, data_len=%d "
		"seq=(%u, %u), ack=%u, flags=0x%x, sacked=0x%x\n",
			tracer ? tracer->queue_mask : 0,
			tracer ? tracer->sock_mask : 0,
			skb->len, skb->data_len,
			TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
			TCP_SKB_CB(skb)->ack_seq,
			TCP_SKB_CB(skb)->tcp_flags, TCP_SKB_CB(skb)->sacked);
}

STATIC void MOCKABLE(skb_tracer_print_skb_queue)(struct sk_buff_head *queue)
{
	unsigned long flags;
	struct sk_buff *skb, *tmp;

	if (skb_queue_empty(queue))
		return;

	spin_lock_irqsave(&queue->lock, flags);

	pr_info("write_queue len=%d\n", queue->qlen);

	skb = skb_peek(queue);
	skb_queue_walk_from_safe(queue, skb, tmp) {
		struct skb_tracer *tracer = skb_ext_find(skb, SKB_TRACER);
		if (tracer)
			skb_tracer_print_skb(tracer, skb);
	}

	spin_unlock_irqrestore(&queue->lock, flags);
}

STATIC void MOCKABLE(skb_tracer_print_skb_rb_tree)(struct rb_root *queue)
{
	struct sk_buff *skb, *tmp;

	if (RB_EMPTY_ROOT(queue))
		return;

	pr_info("rtx_queue\n");

	skb = skb_rb_first(queue);
	skb_rbtree_walk_from_safe(skb, tmp) {
		struct skb_tracer *tracer = skb_ext_find(skb, SKB_TRACER);
		if (tracer)
			skb_tracer_print_skb(tracer, skb);
	}
}

STATIC u64 MOCKABLE(skb_tracer_gen_mask)(struct sock *sk)
{
	unsigned long flags;
	u64 new_mask;

	spin_lock_irqsave(&skb_tracer_sock_mask_lock, flags);

	new_mask = skb_tracer_sock_mask & (-skb_tracer_sock_mask);
	skb_tracer_sock_mask ^= new_mask;

	spin_unlock_irqrestore(&skb_tracer_sock_mask_lock, flags);

	return new_mask;
}

STATIC int find_sk(const void *v, struct file *file, unsigned int n)
{
	struct sock *sk = (void *)v;

	if (file == sk->sk_socket->file)
		return n;

	return 0;
}

STATIC int fd_from_sk(struct sock *sk, struct task_struct *task)
{
	unsigned int fd = 0;

	task_lock(task);
	fd = iterate_fd(task->files, fd, find_sk, sk);
	task_unlock(task);

	return fd;
}

STATIC void MOCKABLE(print_sock_info)(struct sock *sk, struct task_struct *task)
{
	struct socket *sock = sk->sk_socket;
	struct inet_sock *inet = inet_sk(sk);

	pr_info("sk(fd:%d) on %s(pid=%d, uid=%d, gid=%d) port(%d, %d) by 0x%llx\n",
			fd_from_sk(sk, task),
			task->comm, task->pid, sock->file->f_inode->i_uid.val,
			sock->file->f_inode->i_gid.val,
			ntohs(inet->inet_sport), ntohs(inet->inet_dport),
			sk->sk_tracer_mask);
}

void skb_tracer_sk_error_report(struct sock *sk)
{
	if (!sk->sk_tracer_mask)
		return;

	pr_info("sk: mask=0x%llx, sock error=%d\n",
			sk->sk_tracer_mask, sk->sk_err);

	skb_tracer_print_skb_queue(&sk->sk_write_queue);
	skb_tracer_print_skb_rb_tree(&sk->tcp_rtx_queue);
}
EXPORT_SYMBOL(skb_tracer_sk_error_report);

void skb_tracer_init(struct sock *sk)
{
	pid_t pid;
	struct task_struct *task;

	if (!sk->sk_socket)
		return;

	task = current;
	pid = current->pid;

	if (sk->sk_tracer_mask > 0) {
		pr_err("sk(fd:%d) already has mask, 0x%llx\n",
			fd_from_sk(sk, task), sk->sk_tracer_mask);
		return;
	}

#if !IS_ENABLED(CONFIG_KUNIT)
	if (!task || ((!strstr(task->comm, "ims")
			&& !strstr(task->comm, "Ims")
			&& !strstr(task->comm, "IMS")))) {
		return;
	}
#endif
	sk->sk_tracer_mask = skb_tracer_gen_mask(sk);

	print_sock_info(sk, task);
}
EXPORT_SYMBOL(skb_tracer_init);
