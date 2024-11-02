// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
#include <linux/sockptr.h>
#endif
#include <net/tcp.h>
#include "is-common-config.h"
#include "is-debug.h"

#define INADDR_RNDIS	0xc0a82a81 /* 192.168.42.129 */
#define INADDR_ADB	INADDR_LOOPBACK

/*
 * callback sequence
 * connecting: on_accept -> on_connect -> on_open
 * disconnecting: on_close
 */

#define PSC_CONNECTED	1
struct pablo_sock_con {
	struct list_head list;
	struct socket *sock;
	unsigned long flags;
	struct pablo_sock_server *server;
	void *priv;

	/* IS socket connection ops */
	int (*on_open)(struct pablo_sock_con *con);
	void (*on_close)(struct pablo_sock_con *con);
};

#define IS_SOCK_SERVER_NAME_LEN	32
struct pablo_sock_server {
	char name[IS_SOCK_SERVER_NAME_LEN];
	int type;
	unsigned int ip;
	int port;
	bool blocking_accept;
	void *priv;

	struct socket *sock;
	struct task_struct *thread;
	atomic_t num_of_cons;
	struct list_head cons;
	spinlock_t slock;

	/* IS socket server ops */
	int (*on_accept)(struct socket *newsock);
	int (*on_connect)(struct pablo_sock_con *con);
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
static inline int pablo_setsockopt(struct socket *sock, int optname,
					char *optval, unsigned int optlen)
{
	return sock_setsockopt(sock, SOL_SOCKET, optname,
				KERNEL_SOCKPTR((void *)optval), optlen);
}

static inline int pablo_sock_set_reuseaddr(struct socket *s)
{
	sock_set_reuseaddr(s->sk);

	return 0;
}

static inline int pablo_tcp_sock_set_nodelay(struct socket *s)
{
	tcp_sock_set_nodelay(s->sk);

	return 0;
}

static inline int pablo_tcp_sock_set_cork(struct socket *s)
{
	tcp_sock_set_cork(s->sk, false);

	return 0;
}
#else
static inline int pablo_setsockopt(struct socket *sock, int optname,
					char *optval, unsigned int optlen)
{
	mm_segment_t oldfs = get_fs();
	char __user *uoptval;
	int err;

	uoptval = (char __user __force *)optval;

	set_fs(KERNEL_DS);
	err = sock_setsockopt(sock, SOL_SOCKET, optname, uoptval, optlen);
	set_fs(oldfs);

	return err;
}

static inline int pablo_sock_set_reuseaddr(struct socket *s)
{
	int opt = 1;

	return pablo_setsockopt(s, SO_REUSEADDR, (char *)&opt, sizeof(opt));
}

static inline int pablo_tcp_sock_set_nodelay(struct socket *s)
{
	mm_segment_t oldfs = get_fs();
	int optval = 1;
	char __user *uoptval = (char *)&optval;
	int err;

	set_fs(KERNEL_DS);
	err = tcp_setsockopt(s->sk, SOL_TCP, TCP_NODELAY, uoptval, sizeof(uoptval));
	set_fs(oldfs);

	return err;
}

static inline int pablo_tcp_sock_set_cork(struct socket *s)
{
	mm_segment_t oldfs = get_fs();
	int optval = 0;
	char __user *uoptval = (char *)&optval;
	int err;

	set_fs(KERNEL_DS);
	err = tcp_setsockopt(s->sk, SOL_TCP, TCP_CORK, uoptval, sizeof(uoptval));
	set_fs(oldfs);

	return err;
}
#endif

/* IS socket functions */
static int pablo_sock_create(struct socket **sockp, int type, unsigned int ip, int port)
{
	struct sockaddr_in addr;
	struct socket *sock;
	int ret;

	ret = sock_create_kern(&init_net, PF_INET, type, 0, &sock);
	*sockp = sock;
	if (ret) {
		err("failed to create socket: %d", ret);
		return ret;
	}

	/* TODO: SO_KEEPALIVE, SO_LINGER */
	ret = pablo_sock_set_reuseaddr(sock);
	if (ret) {
		err("failed to set SO_REUSEADDR for socket: %d", ret);
		goto err_so_reuseaddr;
	}

	if (type == SOCK_STREAM) {
		ret = pablo_tcp_sock_set_nodelay(sock);
		if (ret) {
			err("failed to set TCP_NODELAY for socket: %d", ret);
			goto err_tcp_nodelay;
		}

		ret = pablo_tcp_sock_set_cork(sock);
		if (ret) {
			err("failed to unset TCP_CORK for socket: %d", ret);
			goto err_tcp_cork;
		}
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = (ip) ? htonl(ip) : htonl(INADDR_ANY);

	ret = kernel_bind(sock, (struct sockaddr *)&addr,
			sizeof(addr));

	if (ret == -EADDRINUSE) {
		err("port %d already in use", port);
		goto err_addr_used;
	}

	if (ret) {
		err("error trying to bind to port %d: %d",
				port, ret);
		goto err_bind;
	}

	return 0;

err_bind:
err_addr_used:
err_so_reuseaddr:
err_tcp_nodelay:
err_tcp_cork:
	sock_release(sock);

	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
static inline int pablo_sock_set_rcvbuf(struct socket *s, int v)
{
	sock_set_rcvbuf(s->sk, v);

	return 0;
}
#else
static inline int pablo_sock_set_rcvbuf(struct socket *s, int v)
{
	int opt = v;

	return pablo_setsockopt(s, SO_RCVBUF, (char *)&opt, sizeof(opt));
}
#endif

static int pablo_sock_setbuf_size(struct socket *sock, int txsize, int rxsize)
{
	int opt;
	int ret;

	if (txsize) {
		opt = txsize;
		ret = pablo_setsockopt(sock, SO_SNDBUF, (char *)&opt, sizeof(opt));
		if (ret) {
			err("failed to set send buffer size: %d", ret);
			return ret;
		}
	}

	if (rxsize) {
		ret = pablo_sock_set_rcvbuf(sock, rxsize);
		if (ret) {
			err("failed to set receive buffer size: %d", ret);
			return ret;
		}
	}

	return 0;
}

static int pablo_sock_create_listen(struct socket **sockp, int type, unsigned int ip, int port)
{
	int ret;

	ret = pablo_sock_create(sockp, type, ip, port);
	if (ret) {
		err("can't create socket");
		return ret;
	}

	ret = kernel_listen(*sockp, 5);
	if (ret) {
		err("failed to listen socket: %d", ret);
		sock_release(*sockp);
	}

	return 0;
}

static int pablo_sock_send_stream(struct socket *sock, void *buf, int len, int timeout)
{
	int ret;
	long ticks = timeout * HZ;
	unsigned long then;
	struct __kernel_sock_timeval tv;

	while (1) {
		struct kvec iov = {
			.iov_base = buf,
			.iov_len = len,
		};

		struct msghdr msg = {
			.msg_flags = (timeout == 0) ? MSG_DONTWAIT : 0,
		};

		if (timeout) {
			tv.tv_sec = timeout / 1000;
			tv.tv_usec = (timeout % 1000) * 1000;

			ret = pablo_setsockopt(sock, SO_SNDTIMEO_NEW, (char *)&tv, sizeof(tv));
			if (ret) {
				err("failed to set SO_SNDTIMEO(%ld.%06d) for socket: %d",
						(long)tv.tv_sec, (int)tv.tv_usec, ret);
				return ret;
			}
		}

		then = jiffies;
		ret = kernel_sendmsg(sock, &msg, &iov, 1, len);
		if (timeout)
			ticks -= jiffies - then;

		if (ret == len)
			return 0;

		if (ret < 0) {
			if (ret == -EAGAIN)
				continue;
			else
				return ret;
		}

		if (ret == 0) {
			err("Software caused connection abort");
			return -ECONNABORTED;
		}

		if (timeout && (ticks <= 0))
			return -EAGAIN;

		buf = ((char *)buf) + ret;
		len -= ret;
	}

	return 0;
}

static int pablo_sock_recv_stream(struct socket *sock, void *buf, int len, int timeout)
{
	int ret;
	long ticks = timeout * HZ;
	unsigned long then;
	struct __kernel_sock_timeval tv;

	while (1) {
		struct kvec iov = {
			.iov_base = buf,
			.iov_len = len,
		};

		struct msghdr msg = {
			.msg_flags = 0,
		};

		if (timeout) {
			tv.tv_sec = timeout / 1000;
			tv.tv_usec = (timeout % 1000) * 1000;

			ret = pablo_setsockopt(sock, SO_RCVTIMEO_NEW, (char *)&tv, sizeof(tv));
			if (ret) {
				err("failed to set SO_RCVTIMEO(%ld.%06d) for socket: %d",
						(long)tv.tv_sec, (int)tv.tv_usec, ret);
				return ret;
			}
		}

		then = jiffies;
		ret = kernel_recvmsg(sock, &msg, &iov, 1, len, 0);
		if (timeout)
			ticks -= jiffies - then;

		if (ret < 0) {
			if (ret == -EAGAIN)
				continue;
			else
				return ret;
		}

		if (ret == 0) {
			err("Connection reset by peer");
			return -ECONNRESET;
		}

		buf = ((char *)buf) + ret;
		len -= ret;

		if (len == 0)
			return 0;

		if (timeout && (ticks <= 0))
			return -ETIMEDOUT;
	}
}

/* IS socket connection functions */
static int pablo_sock_con_open(struct pablo_sock_con **newconp, struct socket *newsock,
		struct pablo_sock_server *svr)
{
	struct pablo_sock_con *newcon;
	int ret;

	newcon = kzalloc(sizeof(*newcon), GFP_KERNEL);
	if (!newcon) {
		err("failed to alloc new connection for %s", svr->name);
		return -ENOMEM;
	}

	newcon->sock = newsock;
	newcon->server = svr;

	if (svr->on_connect) {
		ret = svr->on_connect(newcon);
		if (ret) {
			err("connect callback error: %d", ret);
			kfree(newcon);
			return ret;
		}
	}

	spin_lock(&svr->slock);
	atomic_inc(&svr->num_of_cons);
	list_add_tail(&newcon->list, &svr->cons);
	spin_unlock(&svr->slock);

	if (newcon->on_open) {
		ret = newcon->on_open(newcon);
		if (ret) {
			err("open callback error: %d", ret);
			spin_lock(&svr->slock);
			atomic_dec(&svr->num_of_cons);
			list_del(&newcon->list);
			spin_unlock(&svr->slock);
			kfree(newcon);
			return ret;
		}
	}

	set_bit(PSC_CONNECTED, &newcon->flags);

	*newconp = newcon;

	return 0;
}

static void pablo_sock_con_close(struct pablo_sock_con *con)
{
	if (con && test_and_clear_bit(PSC_CONNECTED, &con->flags)) {
		if (con->on_close)
			con->on_close(con);

		kernel_sock_shutdown(con->sock, SHUT_RDWR);
		sock_release(con->sock);

		spin_lock(&con->server->slock);
		atomic_dec(&con->server->num_of_cons);
		list_del(&con->list);
		spin_unlock(&con->server->slock);

		kfree(con);
	}
}

static int pablo_sock_con_send_stream(struct pablo_sock_con *con, void *buf, int len, int timeout)
{
	int ret;

	if (!test_bit(PSC_CONNECTED, &con->flags))
		return -ENOTCONN;

	ret = pablo_sock_send_stream(con->sock, buf, len, timeout);
	if (ret) {
		err("send stream error: %d", ret);

		if (ret == -EWOULDBLOCK || ret == -EAGAIN)
			return ret;

		pablo_sock_con_close(con);
		return ret;
	}

	return ret;
}

static int pablo_sock_con_recv_stream(struct pablo_sock_con *con, void *buf, int len, int timeout)
{
	int ret;

	if (!test_bit(PSC_CONNECTED, &con->flags))
		return -ENOTCONN;

	ret = pablo_sock_recv_stream(con->sock, buf, len, timeout);
	if (ret) {
		err("recv stream error: %d", ret);

		if (ret == -EWOULDBLOCK || ret == -ETIMEDOUT)
			return ret;

		pablo_sock_con_close(con);
		return ret;
	}

	return ret;
}

/* IS socket server functions */
static int pablo_sock_server_accept(struct pablo_sock_server *svr)
{
	struct socket *sock = svr->sock;
	struct socket *newsock;
	struct pablo_sock_con *newcon;
	int ret;

	ret = kernel_accept(sock, &newsock, svr->blocking_accept ? 0 : O_NONBLOCK);
	if (ret < 0)
		return ret;

	if (svr->on_accept) {
		ret = svr->on_accept(newsock);
		if (ret) {
			err("accept callback error: %d", ret);
			sock_release(newsock);
			return ret;
		}
	}

	ret = pablo_sock_con_open(&newcon, newsock, svr);
	if (ret) {
		err("failed to open new connection for %s", svr->name);
		sock_release(newsock);
		return ret;
	}

	return ret;
}

static int pablo_sock_server_thread(void *data)
{
	struct pablo_sock_server *svr = (struct pablo_sock_server *)data;
	int ret;
	long timeo = msecs_to_jiffies(200);

	ret = pablo_sock_create_listen(&svr->sock, svr->type, svr->ip, svr->port);
	if (ret)
		return ret;

	while (!kthread_should_stop()) {
		ret = pablo_sock_server_accept(svr);
		if (ret == -EAGAIN) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(timeo);
			set_current_state(TASK_RUNNING);
		} else if (ret) {
			err("server accept error: %d", ret);
		}
	}

	kernel_sock_shutdown(svr->sock, SHUT_RDWR);
	sock_release(svr->sock);

	return 0;
}

static int pablo_sock_server_start(struct pablo_sock_server *svr)
{
	int ret = 0;

	svr->thread = kthread_run(pablo_sock_server_thread,
			(void *)svr, "sock-%s", svr->name);
	if (IS_ERR(svr->thread)) {
		err("failed to start socket server thread for %s", svr->name);
		ret = PTR_ERR(svr->thread);
		svr->thread = NULL;
		goto err_run_server_thread;
	}

	atomic_set(&svr->num_of_cons, 0);
	INIT_LIST_HEAD(&svr->cons);
	spin_lock_init(&svr->slock);

err_run_server_thread:
	return ret;
}

static int pablo_sock_server_stop(struct pablo_sock_server *svr)
{
	struct pablo_sock_con *con, *temp;
	int ret;

	if (!svr)
		return -EINVAL;

	if (svr->thread)
		ret = kthread_stop(svr->thread);
	svr->thread = NULL;

	list_for_each_entry_safe(con, temp, &svr->cons, list)
		pablo_sock_con_close(con);

	return 0;
}

/* for tuning interface */
#define TUNING_PORT	5672
#define TUNING_BUF_SIZE SZ_512K

#define TUNING_CONTROL	0
#define TUNING_JSON	1
#define TUNING_IMAGE	2
#define TUNING_MAX	3

struct pablo_sock_server *tuning_svr;
struct mutex ts_mlock;

struct pablo_tuning_con {
	char read_buf[TUNING_BUF_SIZE];
	char write_buf[TUNING_BUF_SIZE];
	struct file *filp;
};

struct pablo_tuning_svr {
	atomic_t num_of_filp;
	struct file *filp[TUNING_MAX];
};

int pablo_tuning_on_open(struct pablo_sock_con *con)
{
	struct pablo_tuning_con *ptc;
	struct pablo_tuning_svr *its = (struct pablo_tuning_svr *)tuning_svr->priv;
	unsigned int cur_con_idx;
	int ret;

	ptc = vzalloc(sizeof(*ptc));
	if (!ptc) {
		err("failed to alloc connection private data");
		return -ENOMEM;
	}
	con->priv = (void *)ptc;

	ret = pablo_sock_setbuf_size(con->sock, TUNING_BUF_SIZE, TUNING_BUF_SIZE);
	if (ret) {
		err("failed to set buf size for %p to %d", con, TUNING_BUF_SIZE);
		goto err_set_buf_size;
	}

	/* a number of connection includes new connecting socket */
	cur_con_idx = atomic_read(&con->server->num_of_cons) - 1;

	if (cur_con_idx >= TUNING_MAX) {
		err("a number of con. is exceed: %d", TUNING_MAX);
		ret = -EMFILE;
		goto err_too_many_con;
	}

	if (its->filp[cur_con_idx]) {
		ptc->filp = its->filp[cur_con_idx];
		ptc->filp->private_data = con;
	} else {
		err("there is no valid opened file for %d", cur_con_idx);
		ret = -ENOENT;
		goto err_invalid_filp;
	}

	return 0;

err_invalid_filp:
err_too_many_con:
err_set_buf_size:
	vfree(ptc);

	return ret;
}

void pablo_tuning_on_close(struct pablo_sock_con *con)
{
	struct pablo_tuning_con *ptc;

	if (con) {
		ptc = (struct pablo_tuning_con *)con->priv;
		ptc->filp->private_data = NULL;
		vfree(ptc);
	}
}

int pablo_tuning_on_accept(struct socket *newsock)
{
	return 0;
}

int pablo_tuning_on_connect(struct pablo_sock_con *newcon)
{
	newcon->on_open = pablo_tuning_on_open;
	newcon->on_close = pablo_tuning_on_close;

	return 0;
}

int pablo_tuning_start(void)
{
	struct pablo_tuning_svr *its;
	int ret;

	tuning_svr = kzalloc(sizeof(*tuning_svr), GFP_KERNEL);
	if (!tuning_svr) {
		err("failed to alloc tuning server");
		return -ENOMEM;
	}

	strncpy(tuning_svr->name, "tuning", sizeof(tuning_svr->name) - 1);
	tuning_svr->type = SOCK_STREAM;
	tuning_svr->ip = INADDR_ADB;
	tuning_svr->port = TUNING_PORT;
	tuning_svr->blocking_accept = false;

	tuning_svr->on_accept = pablo_tuning_on_accept;
	tuning_svr->on_connect = pablo_tuning_on_connect;

	its = kzalloc(sizeof(*its), GFP_KERNEL);
	if (!its) {
		err("failed to alloc tuning server private data");
		kfree(tuning_svr);
		return -ENOMEM;
	}
	tuning_svr->priv = (void *)(its);
	atomic_set(&its->num_of_filp, 0);

	ret = pablo_sock_server_start(tuning_svr);
	if (ret) {
		err("failed to start socket server for %s", tuning_svr->name);
		kfree(its);
		kfree(tuning_svr);
		tuning_svr = NULL;
	}

	return ret;
}

void pablo_tuning_stop(void *data)
{
	if (tuning_svr) {
		pablo_sock_server_stop(tuning_svr);
		kfree(tuning_svr->priv);
		kfree(tuning_svr);
		tuning_svr = NULL;
	}
}

static ssize_t pablo_tuning_read(struct file *filp, char __user *buf,
		size_t len, loff_t *ppos)
{
	struct pablo_sock_con *con = (struct pablo_sock_con *)filp->private_data;
	struct pablo_sock_con *other_con, *temp;
	struct pablo_tuning_con *ptc;
	struct pablo_sock_server *server;
	int ret;

	if (!con)
		return -EINVAL;

	if (!test_bit(PSC_CONNECTED, &con->flags))
		return -ENOTCONN;

	ptc = (struct pablo_tuning_con *)con->priv;
	server = con->server;

	if (len > TUNING_BUF_SIZE)
		len = TUNING_BUF_SIZE;

	ret = pablo_sock_con_recv_stream(con, ptc->read_buf, len, 0);
	if (ret) {
		/*
		 * We can realize that the client was diconnected with this error.
		 * Any connected sockets which are waiting for client to receivce
		 * messages could get this error due to client's disconnection.
		 * So, we need to reset all connected sockets to restart.
		 */
		if (ret == -ECONNRESET) {
			list_for_each_entry_safe(other_con, temp,
					&server->cons, list)
				pablo_sock_con_close(other_con);
		}

		return ret;
	}

	ret = copy_to_user(buf, ptc->read_buf, len);
	if (ret) {
		err("failed to copy to user: %d", ret);
		return -EPERM;
	}

	return len;
}

static ssize_t pablo_tuning_write(struct file *filp, const char __user *buf,
		size_t len, loff_t *ppos)
{
	struct pablo_sock_con *con = (struct pablo_sock_con *)filp->private_data;
	struct pablo_tuning_con *ptc;
	int ret;

	if (!con)
		return -EINVAL;

	if (!test_bit(PSC_CONNECTED, &con->flags))
		return -ENOTCONN;

	ptc = (struct pablo_tuning_con *)con->priv;

	if (len > TUNING_BUF_SIZE)
		len = TUNING_BUF_SIZE;

	ret = copy_from_user(ptc->write_buf, buf, len);
	if (ret) {
		err("failed to copy from user: %d", ret);
		return -EPERM;
	}

	ret = pablo_sock_con_send_stream(con, ptc->write_buf, len, 0);
	if (ret)
		return ret;

	return len;
}

static int pablo_tuning_open(struct inode *inode, struct file *filp)
{
	struct pablo_tuning_svr *its;
	unsigned int num_of_filp;
	int ret;


	mutex_lock(&ts_mlock);

	if (!tuning_svr) {
		ret = pablo_tuning_start();
		if (ret) {
			err("failed to start tuning server: %d", ret);
			mutex_unlock(&ts_mlock);
			return ret;
		}
	}

	its = (struct pablo_tuning_svr *)tuning_svr->priv;
	num_of_filp = atomic_read(&its->num_of_filp);
	if (num_of_filp >= TUNING_MAX) {
		mutex_unlock(&ts_mlock);
		return -EINVAL;
	}

	its->filp[num_of_filp] = filp;
	atomic_inc(&its->num_of_filp);

	filp->private_data = NULL;

	mutex_unlock(&ts_mlock);

	return 0;
}

static int pablo_tuning_close(struct inode *inodep, struct file *filp)
{
	struct pablo_sock_con *con = (struct pablo_sock_con *)filp->private_data;
	struct pablo_tuning_svr *its = (struct pablo_tuning_svr *)tuning_svr->priv;


	mutex_lock(&ts_mlock);

	if (con)
		pablo_sock_con_close(con);

	if (!atomic_dec_return(&its->num_of_filp))
		pablo_tuning_stop(NULL);

	mutex_unlock(&ts_mlock);

	return 0;
}

static const struct file_operations pablo_tuning_fops = {
	.owner		= THIS_MODULE,
	.read		= pablo_tuning_read,
	.write		= pablo_tuning_write,
	.open		= pablo_tuning_open,
	.release	= pablo_tuning_close,
	.llseek		= no_llseek,
};

struct miscdevice pablo_tuning_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "pablo_tuning_socket",
	.fops	= &pablo_tuning_fops,
};

int pablo_sock_init(void)
{
	int ret;

	/* tuning misc device */
	ret = misc_register(&pablo_tuning_device);
	if (ret)
		err("failed to register pablo_tuning_socket devices");

	mutex_init(&ts_mlock);

	return ret;
}

void pablo_sock_exit(void)
{
	/* tuning misc device */
	misc_deregister(&pablo_tuning_device);
}

#ifndef MODULE
module_init(pablo_sock_init);
module_exit(pablo_sock_exit);
#endif

MODULE_AUTHOR("Jeongtae Park <jtp.park@samsung.com>");
MODULE_DESCRIPTION("Socket layer for Pablo");
MODULE_LICENSE("GPL");
