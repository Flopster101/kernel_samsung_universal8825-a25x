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

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/debug-snapshot.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos-s2mpu.h>
#endif
#include "gnss_prj.h"
#include "gnss_utils.h"

#define WAKE_TIME   (HZ/2) /* 500 msec */

static inline void iodev_lock_wlock(struct io_device *iod)
{
	if (iod->waketime > 0 && !gnssif_wake_lock_active(iod->ws)) {
		gnssif_wake_unlock(iod->ws);
		gnssif_wake_lock_timeout(iod->ws, iod->waketime);
	}
}

static inline int queue_skb_to_iod(struct sk_buff *skb, struct io_device *iod)
{
	struct sk_buff_head *rxq = &iod->sk_rx_q;

	if (rxq->qlen > MAX_IOD_RXQ_LEN) {
		gif_err_limited("%s: %s application may be dead (rxq->qlen %d > %d)\n",
			iod->name, iod->app ? iod->app : "corresponding",
			rxq->qlen, MAX_IOD_RXQ_LEN);
		dev_kfree_skb_any(skb);
		return -ENOSPC;
	}

	skb_queue_tail(rxq, skb);
	gif_debug("%s: rxq->qlen = %d\n", iod->name, rxq->qlen);
	wake_up(&iod->wq);

	return 0;
}

static inline int rx_frame_with_link_header(struct sk_buff *skb)
{
	struct exynos_link_header *hdr;

	/* Remove EXYNOS link header */
	hdr = (struct exynos_link_header *)skb->data;
	skb_pull(skb, EXYNOS_HEADER_SIZE);

#if defined(DEBUG_GNSS_IPC_PKT)
	/* Print received data from GNSS */
	gnss_log_ipc_pkt(skb, RX);
#endif

	return queue_skb_to_iod(skb, skbpriv(skb)->iod);
}

static int rx_fmt_ipc(struct sk_buff *skb)
{
	return rx_frame_with_link_header(skb);
}

static int rx_demux(struct link_device *ld, struct sk_buff *skb)
{
	struct io_device *iod;

	iod = ld->iod;
	if (unlikely(!iod)) {
		gif_err("%s: ERR! no iod!\n", ld->name);
		return -ENODEV;
	}

	skbpriv(skb)->ld = ld;
	skbpriv(skb)->iod = iod;

	if (atomic_read(&iod->opened) <= 0) {
		gif_err_limited("%s: ERR! %s is not opened\n", ld->name, iod->name);
		return -ENODEV;
	}

	return rx_fmt_ipc(skb);
}

static int rx_frame_done(struct io_device *iod, struct link_device *ld,
		struct sk_buff *skb)
{
	/* Cut off the padding of the current frame */
	skb_trim(skb, exynos_get_frame_len(skb->data));
	gif_debug("%s->%s: frame length = %d\n", ld->name, iod->name, skb->len);

	return rx_demux(ld, skb);
}

static int recv_frame_from_skb(struct io_device *iod, struct link_device *ld,
		struct sk_buff *skb)
{
	struct sk_buff *clone;
	unsigned int rest;
	unsigned int rcvd;
	unsigned int tot;		/* total length including padding */
	int err = 0;

	/*
	 * If there is only one EXYNOS frame in @skb, receive the EXYNOS frame and
	 * return immediately. In this case, the frame verification must already
	 * have been done at the link device.
	 */
	if (skbpriv(skb)->single_frame) {
		err = rx_frame_done(iod, ld, skb);
		if (err < 0)
			goto exit;
		return 0;
	}

	/*
	 * The routine from here is used only if there may be multiple EXYNOS
	 * frames in @skb.
	 */

	/* Check the config field of the first frame in @skb */
	if (!exynos_start_valid(skb->data)) {
		gif_err("%s->%s: ERR! INVALID config 0x%02X\n",
			ld->name, iod->name, skb->data[0]);
		err = -EINVAL;
		goto exit;
	}

	/* Get the total length of the frame with a padding */
	tot = exynos_get_total_len(skb->data);

	/* Verify the total length of the first frame */
	rest = skb->len;
	if (unlikely(tot > rest)) {
		gif_err("%s->%s: ERR! tot %d > skb->len %d)\n",
			ld->name, iod->name, tot, rest);
		err = -EINVAL;
		goto exit;
	}

	/* If there is only one EXYNOS frame in @skb, */
	if (likely(tot == rest)) {
		/* Receive the EXYNOS frame and return immediately */
		err = rx_frame_done(iod, ld, skb);
		if (err < 0)
			goto exit;
		return 0;
	}

	/* This routine is used only if there are multiple EXYNOS frames in @skb */
	rcvd = 0;
	while (rest > 0) {
		clone = skb_clone(skb, GFP_ATOMIC);
		if (unlikely(!clone)) {
			gif_err("%s->%s: ERR! skb_clone fail\n",
				ld->name, iod->name);
			err = -ENOMEM;
			goto exit;
		}

		/* Get the start of an EXYNOS frame */
		skb_pull(clone, rcvd);
		if (!exynos_start_valid(clone->data)) {
			gif_err("%s->%s: ERR! INVALID config 0x%02X\n",
				ld->name, iod->name, clone->data[0]);
			dev_kfree_skb_any(clone);
			err = -EINVAL;
			goto exit;
		}

		/* Get the total length of the current frame with a padding */
		tot = exynos_get_total_len(clone->data);
		if (unlikely(tot > rest)) {
			gif_err("%s->%s: ERR! dirty frame (tot %d > rest %d)\n",
				ld->name, iod->name, tot, rest);
			dev_kfree_skb_any(clone);
			err = -EINVAL;
			goto exit;
		}

		/* Cut off the padding of the current frame */
		skb_trim(clone, exynos_get_frame_len(clone->data));

		/* Demux the frame */
		err = rx_demux(ld, clone);
		if (err < 0) {
			gif_err("%s->%s: ERR! rx_demux fail (err %d)\n",
				ld->name, iod->name, err);
			dev_kfree_skb_any(clone);
			goto exit;
		}

		/* Calculate the start of the next frame */
		rcvd += tot;

		/* Calculate the rest size of data in @skb */
		rest -= tot;
	}

exit:
	dev_kfree_skb_any(skb);
	return err;
}

/* called from link device when a packet arrives for this io device */
static int io_dev_recv_skb_from_link_dev(struct io_device *iod,
		struct link_device *ld, struct sk_buff *skb)
{
	int err;

	iodev_lock_wlock(iod);

	err = recv_frame_from_skb(iod, ld, skb);
	if (err < 0) {
		gif_err("%s->%s: ERR! recv_frame_from_skb fail(err %d)\n",
				ld->name, iod->name, err);
	}

	return err;
}

/* called from link device when a packet arrives fo this io device */
static int io_dev_recv_skb_single_from_link_dev(struct io_device *iod,
				struct link_device *ld, struct sk_buff *skb)
{
	int err;

	if (unlikely(atomic_read(&iod->opened) <= 0)) {
		gif_err_limited("%s<-%s: ERR! %s is not opened\n",
			iod->name, ld->name, iod->name);
		return -ENODEV;
	}

	iodev_lock_wlock(iod);

	if (skbpriv(skb)->lnk_hdr)
		skb_trim(skb, exynos_get_frame_len(skb->data));

	err = rx_demux(ld, skb);
	if (err < 0)
		gif_err_limited("%s<-%s: ERR! rx_demux fail (err %d)\n",
			iod->name, ld->name, err);

	return err;
}

static int misc_open(struct inode *inode, struct file *filp)
{
	struct io_device *iod = to_io_device(filp->private_data);
	struct link_device *ld;
	int ref_cnt;
	filp->private_data = (void *)iod;

	ld = iod->ld;

	ref_cnt = atomic_inc_return(&iod->opened);

	gif_info("%s (opened %d) by %s\n", iod->name, ref_cnt, current->comm);

	return 0;
}

static int misc_release(struct inode *inode, struct file *filp)
{
	struct io_device *iod = (struct io_device *)filp->private_data;

	if (atomic_dec_and_test(&iod->opened))
		skb_queue_purge(&iod->sk_rx_q);

	gif_info("%s (opened %d) by %s\n", iod->name, atomic_read(&iod->opened), current->comm);

	return 0;
}

static unsigned int misc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct gnss_ctl *gc = iod->gc;
	poll_wait(filp, &iod->wq, wait);

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
	if (gc->is_irq_received == true) {
		gif_err("POLL wakeup for power on/off interrupt\n");
		return POLLPRI;
	}
#endif

	if (!skb_queue_empty(&iod->sk_rx_q) && gc->gnss_state == STATE_ONLINE)
		return POLLIN | POLLRDNORM;

	if (gc->gnss_state == STATE_OFFLINE || gc->gnss_state == STATE_FAULT) {
		gif_err("POLL wakeup in abnormal state!!!\n");
		return POLLHUP;
	} else {
		return 0;
	}
}

static int valid_cmd_arg(unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case GNSS_IOCTL_RESET:
	case GNSS_IOCTL_LOAD_FIRMWARE:
	case GNSS_IOCTL_REQ_FAULT_INFO:
	case GNSS_IOCTL_REQ_BCMD:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		return access_ok((const void *)arg, sizeof(arg));
#else
		return access_ok(VERIFY_READ, (const void *)arg, sizeof(arg));
#endif
	case GNSS_IOCTL_READ_FIRMWARE:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		return access_ok((const void *)arg, sizeof(arg));
#else
		return access_ok(VERIFY_WRITE, (const void *)arg, sizeof(arg));
#endif
	default:
		return true;
	}
}

static int send_bcmd(struct io_device *iod, unsigned long arg)
{
	struct gnss_ctl *gc = iod->gc;
	struct kepler_bcmd_args bcmd_args;
	int err = 0;

	memset(&bcmd_args, 0, sizeof(struct kepler_bcmd_args));
	err = copy_from_user(&bcmd_args, (const void __user *)arg,
			sizeof(struct kepler_bcmd_args));
	if (err) {
		gif_err("copy_from_user fail(to get structure)\n");
		err = -EFAULT;
		goto bcmd_exit;
	}

	if (!gc->ops.req_bcmd) {
		gif_err("%s: !ld->req_bcmd\n", iod->name);
		err = -EFAULT;
		goto bcmd_exit;
	}

	gif_info("flags:%d, cmd_id:%d, param1:0x%08x, param2:%d(0x%08x)\n",
			bcmd_args.flags, bcmd_args.cmd_id, bcmd_args.param1,
			bcmd_args.param2, bcmd_args.param2);

	err = gc->ops.req_bcmd(gc, bcmd_args.cmd_id, bcmd_args.flags,
				bcmd_args.param1, bcmd_args.param2);
	if (err == -EIO) { /* BCMD timeout */
		gif_err("BCMD timeout cmd_id : %d\n", bcmd_args.cmd_id);
	} else if (err == -EPERM) {
		gif_err("BCMD failed due to invalid state\n");
	} else {
		bcmd_args.ret_val = err;
		err = copy_to_user((void __user *)arg,
				(void *)&bcmd_args, sizeof(bcmd_args));
		if (err) {
			gif_err("copy_to_user fail(to send bcmd params)\n");
			err = -EFAULT;
		}
	}

bcmd_exit:
	return err;
}

static int gnss_load_firmware(struct io_device *iod,
		struct kepler_firmware_args firmware_arg)
{
	struct link_device *ld = iod->ld;
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU) && !IS_ENABLED(CONFIG_SOC_S5E8825)
	struct gnss_pdata *pdata = iod->gc->pdata;
	unsigned long ret;
#endif
	int err;

	gif_info("Load Firmware - fw size : %d, fw_offset : %d\n",
			firmware_arg.firmware_size, firmware_arg.offset);

	if (!ld->copy_reserved_from_user) {
		gif_err("No copy_reserved_from_user method\n");
		return -EFAULT;
	}

	err = ld->copy_reserved_from_user(iod->ld, firmware_arg.offset,
			firmware_arg.firmware_bin, firmware_arg.firmware_size);
	if (err) {
		gif_err("Unable to load firmware\n");
		return -EFAULT;
	}

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU) && !IS_ENABLED(CONFIG_SOC_S5E8825)
	ret = exynos_verify_subsystem_fw("GNSS", 0,
					 pdata->shmem_base + pdata->code_offset,
					 firmware_arg.firmware_size, pdata->code_allowed_size);
	if (ret) {
		gif_err("Failed FW verification ret:%lu\n", ret);
		return -EIO;
	}

	ret = exynos_request_fw_stage2_ap("GNSS");
	if (ret) {
		gif_err("Failed stage 2 access permission ret:%lu\n", ret);
		return -EACCES;
	}
#endif

	return 0;
}

static int parsing_load_firmware(struct io_device *iod, unsigned long arg)
{
	struct kepler_firmware_args firmware_arg;
	int err = 0;
	struct gnss_pdata *pdata = iod->gc->pdata;

	memset(&firmware_arg, 0, sizeof(struct kepler_firmware_args));
	err = copy_from_user(&firmware_arg, (const void __user *)arg,
			sizeof(struct kepler_firmware_args));
	if (err) {
		gif_err("copy_from_user fail(to get structure)\n");
		err = -EFAULT;
		return err;
	}
	if ((firmware_arg.offset < pdata->code_offset) ||
		(pdata->code_allowed_size < (firmware_arg.offset - pdata->code_offset))) {
		gif_err("wrong offset to download firmware:0x%x 0x%x 0x%x\n",
			firmware_arg.offset, pdata->code_offset, pdata->code_allowed_size);
		err = -EFAULT;
		return err;
	}
	if (firmware_arg.firmware_size > pdata->code_allowed_size) {
		gif_err("size too big to download:0x%x 0x%x\n",
			firmware_arg.firmware_size, pdata->code_allowed_size);
		err = -EFAULT;
		return err;
	}

	gif_info("FIRMWARE OFFSET: 0x%08x SIZE: 0x%08x\n", firmware_arg.offset,
			firmware_arg.firmware_size);
	return gnss_load_firmware(iod, firmware_arg);
}

static int gnss_load_data(struct io_device *iod,
		struct kepler_data_args data_arg)
{
	struct link_device *ld = iod->ld;
	int err = 0;

	gif_info("Load Configuration Data - size : %d, offset : %d\n",
			data_arg.size, data_arg.offset);

	if (!ld->copy_reserved_from_user) {
		gif_err("No copy_reserved_from_user method\n");
		err = -EFAULT;
		goto load_data_exit;
	}

	err = ld->copy_reserved_from_user(iod->ld, data_arg.offset,
			data_arg.data, data_arg.size);
	if (err) {
		gif_err("Unable to load data\n");
		err = -EFAULT;
		goto load_data_exit;
	}

load_data_exit:
	return err;
}

static int parsing_load_data(struct io_device *iod, unsigned long arg)
{
	struct kepler_data_args data_arg;
	int err = 0;
	struct gnss_pdata *pdata = iod->gc->pdata;

	memset(&data_arg, 0, sizeof(struct kepler_data_args));
	err = copy_from_user(&data_arg, (const void __user *)arg,
			sizeof(struct kepler_data_args));
	if (err) {
		gif_err("copy_from_user fail(to get structure)\n");
		err = -EFAULT;
		return err;
	}
	if ((data_arg.offset < pdata->code_offset) ||
		(pdata->code_allowed_size < (data_arg.offset - pdata->code_offset))) {
		gif_err("wrong offset to download configuration data:0x%x 0x%x 0x%x\n",
			data_arg.offset, pdata->code_offset, pdata->code_allowed_size);
		err = -EFAULT;
		return err;
	}
	if (data_arg.size > pdata->code_allowed_size) {
		gif_err("size too big to download:0x%x 0x%x\n",
			data_arg.size, pdata->code_allowed_size);
		err = -EFAULT;
		return err;
	}

	gif_info("Configuration Data OFFSET: 0x%08x SIZE: 0x%08x\n",
			data_arg.offset, data_arg.size);
	return gnss_load_data(iod, data_arg);
}

static int gnss_read_firmware(struct io_device *iod,
		struct kepler_firmware_args firmware_arg)
{
	struct link_device *ld = iod->ld;
	int err = 0;

	gif_debug("Read Firmware - fw size : %d, fw_offset : %d\n",
			firmware_arg.firmware_size, firmware_arg.offset);

	if (!ld->copy_reserved_to_user) {
		gif_err("No copy_reserved_to_user method\n");
		err = -EFAULT;
		goto read_firmware_exit;
	}

	err = ld->copy_reserved_to_user(iod->ld, firmware_arg.offset,
			firmware_arg.firmware_bin, firmware_arg.firmware_size);
	if (err) {
		gif_err("Unable to read firmware\n");
		err = -EFAULT;
		goto read_firmware_exit;
	}

read_firmware_exit:
	return err;
}

static int parsing_read_firmware(struct io_device *iod, unsigned long arg)
{
	struct kepler_firmware_args firmware_arg;
	int err = 0;

	memset(&firmware_arg, 0, sizeof(struct kepler_firmware_args));
	err = copy_from_user(&firmware_arg, (const void __user *)arg,
			sizeof(struct kepler_firmware_args));
	if (err) {
		gif_err("copy_from_user fail(to get structure)\n");
		err = -EFAULT;
		return err;
	}

	return gnss_read_firmware(iod, firmware_arg);
}

static int change_tcxo_mode(struct gnss_ctl *gc, unsigned long arg)
{
	enum gnss_tcxo_mode tcxo_mode;
	int ret;

	ret = copy_from_user(&tcxo_mode, (const void __user *)arg,
			sizeof(enum gnss_tcxo_mode));
	if (ret) {
		gif_err("copy_from_user fail(to get tcxo mode)\n");
		ret = -EFAULT;
		goto change_mode_exit;
	}

	if (!gc->pmu_ops->change_tcxo_mode) {
		gif_err("func is null\n");
		ret = -EFAULT;
		goto change_mode_exit;
	}

	ret = gc->pmu_ops->change_tcxo_mode(tcxo_mode);

change_mode_exit:
	return ret;
}

static int set_sensor_power(struct gnss_ctl *gc, unsigned long arg)
{
	enum sensor_power sensor_power_en;
	int ret;

	ret = copy_from_user(&sensor_power_en, (const void __user *)arg,
			sizeof(enum sensor_power));
	if (ret) {
		gif_err("copy_from_user fail(to get sensor power setting)\n");
		ret = -EFAULT;
		goto set_sensor_power_exit;
	}

	if (!gc->ops.set_sensor_power) {
		gif_err("func is null\n");
		ret = -EFAULT;
		goto set_sensor_power_exit;
	}

	ret = gc->ops.set_sensor_power(gc, sensor_power_en);

set_sensor_power_exit:
	return ret;
}

static long misc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = iod->ld;
	struct gnss_ctl *gc = iod->gc;
	struct gnss_swreg swreg;
	struct gnss_apreg apreg;
	int err = 0;
	int size;
	int ret = 0;

	if (!valid_cmd_arg(cmd, arg))
		return -ENOTTY;

	switch (cmd) {
	case GNSS_IOCTL_RESET:
		gif_info("%s: GNSS_IOCTL_RESET\n", iod->name);

		if (!gc->ops.gnss_hold_reset) {
			gif_err("%s: !gc->ops.gnss_reset\n", iod->name);
			return -EINVAL;
		}
		ret = gc->ops.gnss_hold_reset(gc);
		skb_queue_purge(&iod->sk_rx_q);
		return ret;

	case GNSS_IOCTL_REQ_FAULT_INFO:
		gif_info("%s: GNSS_IOCTL_REQ_FAULT_INFO\n", iod->name);

		if (!gc->ops.gnss_req_fault_info) {
			gif_err("%s: !gc->ops.req_fault_info\n", iod->name);
			return -EFAULT;
		}
		size = gc->ops.gnss_req_fault_info(gc);

		gif_info("gnss_req_fault_info returned %d\n", size);

		if (size < 0) {
			gif_err("Can't get fault info from Kepler\n");
			return ret;
		}

		if (size > 0) {
			err = ld->dump_fault_to_user(ld, (void __user *)arg, size);
			if (err) {
				gif_err("copy_to_user fail(to copy fault info)\n");
				return -EFAULT;
			}
		}
		return size;

	case GNSS_IOCTL_REQ_BCMD:
		gif_info("%s: GNSS_IOCTL_REQ_BCMD\n", iod->name);
		return send_bcmd(iod, arg);

	case GNSS_IOCTL_LOAD_FIRMWARE:
		gif_info("%s: GNSS_IOCTL_LOAD_FIRMWARE\n", iod->name);
		return parsing_load_firmware(iod, arg);

	case GNSS_IOCTL_LOAD_DATA:
		gif_info("%s: GNSS_IOCTL_LOAD_DATA\n", iod->name);
		return parsing_load_data(iod, arg);

	case GNSS_IOCTL_READ_FIRMWARE:
		gif_debug("%s: GNSS_IOCTL_READ_FIRMWARE\n", iod->name);
		return parsing_read_firmware(iod, arg);

	case GNSS_IOCTL_CHANGE_SENSOR_GPIO:
		gif_info("%s: GNSS_IOCTL_CHANGE_SENSOR_GPIO\n", iod->name);

		if (!gc->ops.change_sensor_gpio) {
			gif_err("%s: !gc->ops.change_sensor_gpio\n", iod->name);
			return -EFAULT;
		}
		return gc->ops.change_sensor_gpio(gc);

	case GNSS_IOCTL_CHANGE_TCXO_MODE:
		gif_info("%s: GNSS_IOCTL_CHANGE_TCXO_MODE\n", iod->name);
		return change_tcxo_mode(gc, arg);

	case GNSS_IOCTL_SET_SENSOR_POWER:
		gif_info("%s: GNSS_IOCTL_SENSOR_POWER\n", iod->name);
		return set_sensor_power(gc, arg);

	case GNSS_IOCTL_SET_WATCHDOG_RESET:
		gif_info("%s: GNSS_IOCTL_SET_WATCHDOG_RESET\n", iod->name);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		return dbg_snapshot_expire_watchdog();
#else
		gif_err("debug snapshot is not enabled\n");
		return -EINVAL;
#endif

	case GNSS_IOCTL_READ_SHMEM_SIZE:
		gif_info("%s: GNSS_IOCTL_READ_SHMEM_SIZE\n", iod->name);
		return gc->pdata->shmem_size;

	case GNSS_IOCTL_READ_RESET_COUNT:
		gif_info("%s: GNSS_IOCTL_READ_RESET_COUNT\n", iod->name);
		return gc->reset_count;

	case GNSS_IOCTL_GET_SWREG:
		gif_info("%s: GNSS_IOCTL_GET_SWREG\n", iod->name);
		if (!gc->pmu_ops->get_swreg) {
			gif_err("get_swreg is not available\n");
			return -EINVAL;
		}
		gc->pmu_ops->get_swreg(&swreg);
		err = copy_to_user((void __user *)arg, &swreg, sizeof(struct gnss_swreg));
		if (err)
			gif_err("copy to user fail for swreg (0x%08x)\n", err);

		return err;

	case GNSS_IOCTL_GET_APREG:
		gif_info("%s: GNSS_IOCTL_GET_APREG\n", iod->name);
		if (!gc->pmu_ops->get_apreg) {
			gif_err("get_apreg is not available\n");
			return -EINVAL;
		}
		gc->pmu_ops->get_apreg(&apreg);
		err = copy_to_user((void __user *)arg, &apreg, sizeof(struct gnss_apreg));
		if (err)
			gif_err("copy to user fail for apreg (0x%08x)\n", err);

		return err;

	case GNSS_IOCTL_RELEASE_RESET:
		gif_info("%s: GNSS_IOCTL_RELEASE_RESET\n", iod->name);

		if (!gc->ops.gnss_release_reset) {
			gif_err("%s: !gc->ops.gnss_release_reset\n", iod->name);
			return -EINVAL;
		}
		ret = gc->ops.gnss_release_reset(gc);
		return ret;

	case GNSS_IOCTL_POWER_ON:
		gif_info("%s: GNSS_IOCTL_POWER_ON\n", iod->name);

		if (!gc->ops.gnss_power_on) {
			gif_err("%s: !gc->ops.gnss_power_on\n", iod->name);
			return -EINVAL;
		}
		ret = gc->ops.gnss_power_on(gc);
		return ret;

	default:
		gif_err("%s: ERR! undefined cmd 0x%X\n", iod->name, cmd);
		return -EINVAL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static int parsing_load_firmware32(struct io_device *iod, unsigned long arg)
{
	struct kepler_firmware_args firmware_arg;
	struct kepler_firmware_args32 arg32;
	int err = 0;

	memset(&firmware_arg, 0, sizeof(firmware_arg));
	err = copy_from_user(&arg32, (const void __user *)arg,
			sizeof(struct kepler_firmware_args32));
	if (err) {
		gif_err("copy_from_user fail(to get structure)\n");
		err = -EFAULT;

		return err;
	}

	firmware_arg.firmware_size = arg32.firmware_size;
	firmware_arg.offset = arg32.offset;
	firmware_arg.firmware_bin = compat_ptr(arg32.firmware_bin);

	return gnss_load_firmware(iod, firmware_arg);
}

static int parsing_read_firmware32(struct io_device *iod, unsigned long arg)
{
	struct kepler_firmware_args firmware_arg;
	struct kepler_firmware_args32 arg32;
	int err = 0;

	memset(&firmware_arg, 0, sizeof(firmware_arg));
	err = copy_from_user(&arg32, (const void __user *)arg,
			sizeof(struct kepler_firmware_args32));
	if (err) {
		gif_err("copy_from_user fail(to get structure)\n");
		err = -EFAULT;

		return err;
	}

	firmware_arg.firmware_size = arg32.firmware_size;
	firmware_arg.offset = arg32.offset;
	firmware_arg.firmware_bin = compat_ptr(arg32.firmware_bin);

	return gnss_read_firmware(iod, firmware_arg);
}

static long misc_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	unsigned long realarg = (unsigned long)compat_ptr(arg);

	if (!valid_cmd_arg(cmd, realarg))
		return -ENOTTY;

	switch (cmd) {
	case GNSS_IOCTL_LOAD_FIRMWARE:
		gif_info("%s: GNSS_IOCTL_LOAD_FIRMWARE (32-bit)\n", iod->name);
		return parsing_load_firmware32(iod, realarg);
	case GNSS_IOCTL_READ_FIRMWARE:
		gif_info("%s: GNSS_IOCTL_READ_FIRMWARE (32-bit)\n", iod->name);
		return parsing_read_firmware32(iod, realarg);
	}
	return misc_ioctl(filp, cmd, realarg);
}
#endif

static void exynos_build_header(struct io_device *iod, struct link_device *ld,
				u8 *buff, u16 cfg, u8 ctl, size_t count)
{
	u16 *exynos_header = (u16 *)(buff + EXYNOS_START_OFFSET);
	u16 *frame_seq = (u16 *)(buff + EXYNOS_FRAME_SEQ_OFFSET);
	u16 *frag_cfg = (u16 *)(buff + EXYNOS_FRAG_CONFIG_OFFSET);
	u16 *size = (u16 *)(buff + EXYNOS_LEN_OFFSET);
	struct exynos_seq_num *seq_num = &(iod->seq_num);

	*exynos_header = EXYNOS_START_MASK;
	*frame_seq = ++seq_num->frame_cnt;
	*frag_cfg = cfg;
	*size = (u16)(EXYNOS_HEADER_SIZE + count);
	buff[EXYNOS_CH_ID_OFFSET] = 0; /* single channel, should be 0. */

	if (cfg == EXYNOS_SINGLE_MASK)
		*frag_cfg = cfg;

	buff[EXYNOS_CH_SEQ_OFFSET] = ++seq_num->ch_cnt[0];
}

static ssize_t misc_write(struct file *filp, const char __user *data,
			size_t count, loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct link_device *ld = iod->ld;
	struct sk_buff *skb;
	u8 *buff;
	int ret;
	size_t headroom;
	size_t tailroom;
	size_t tx_bytes;
	u16 fr_cfg;
	struct gnss_ctl *gc = iod->gc;

	if (gc->gnss_state != STATE_ONLINE) {
		gif_err_limited("%s: ERR! gnss is not online\n", iod->name);
		return SIGHUP;
	}

	fr_cfg = EXYNOS_SINGLE_MASK << 8;
	headroom = EXYNOS_HEADER_SIZE;
	tailroom = exynos_calc_padding_size(EXYNOS_HEADER_SIZE + count);

	tx_bytes = headroom + count + tailroom;

	skb = alloc_skb(tx_bytes, GFP_KERNEL);
	if (!skb) {
		gif_err("%s: ERR! alloc_skb fail (tx_bytes:%ld)\n",
			iod->name, tx_bytes);
		return -ENOMEM;
	}

	/* Store the IO device, the link device, etc. */
	skbpriv(skb)->iod = iod;
	skbpriv(skb)->ld = ld;

	skbpriv(skb)->lnk_hdr = iod->link_header;
	skbpriv(skb)->exynos_ch = 0; /* Single channel should be 0. */

	/* Build EXYNOS link header */
	if (fr_cfg) {
		buff = skb_put(skb, headroom);
		exynos_build_header(iod, ld, buff, fr_cfg, 0, count);
	}

	/* Store IPC message */
	buff = skb_put(skb, count);
	if (copy_from_user(buff, data, count)) {
		gif_err("%s->%s: ERR! copy_from_user fail (count %ld)\n",
			iod->name, ld->name, count);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	/* Apply padding */
	if (tailroom)
		skb_put(skb, tailroom);

	/* send data with sk_buff, link device will put sk_buff
	 * into the specific sk_buff_q and run work-q to send data
	 */
	skbpriv(skb)->iod = iod;
	skbpriv(skb)->ld = ld;

	ret = ld->send(ld, iod, skb);
	if (ret < 0) {
		gif_err("%s->%s: ERR! ld->send fail (err %d, tx_bytes %ld)\n",
			iod->name, ld->name, ret, tx_bytes);
		return ret;
	}

	if (ret != tx_bytes) {
		gif_debug("%s->%s: WARNING! ret %d != tx_bytes %ld (count %ld)\n",
			iod->name, ld->name, ret, tx_bytes, count);
	}

	return count;
}

static ssize_t misc_read(struct file *filp, char *buf, size_t count,
			loff_t *fpos)
{
	struct io_device *iod = (struct io_device *)filp->private_data;
	struct sk_buff_head *rxq = &iod->sk_rx_q;
	struct sk_buff *skb;
	int copied = 0;
	struct gnss_ctl *gc = iod->gc;

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_MBIM)
	if (gc->is_irq_received == true) {
		gc->is_irq_received = false;
		if (copy_to_user(buf, &gc->gnss_pwr, sizeof(unsigned int))) {
			gif_err("%s: ERR! copy_to_user fail\n", iod->name);
			return -EFAULT;
		}
		return 0;
	}
#endif

	if (gc->gnss_state != STATE_ONLINE) {
		gif_err("%s: ERR! gnss is not online\n", iod->name);
		return SIGHUP;
	}

	if (skb_queue_empty(rxq)) {
		gif_debug("%s: ERR! no data in rxq\n", iod->name);
		return 0;
	}

	skb = skb_dequeue(rxq);
	if (unlikely(!skb)) {
		gif_debug("%s: No data in RXQ\n", iod->name);
		return 0;
	}

	copied = skb->len > count ? count : skb->len;

	if (copy_to_user(buf, skb->data, copied)) {
		gif_err("%s: ERR! copy_to_user fail\n", iod->name);
		dev_kfree_skb_any(skb);
		return -EFAULT;
	}

	gif_debug("%s: data:%d copied:%d qlen:%d\n",
			iod->name, skb->len, copied, rxq->qlen);

	if (skb->len > count) {
		skb_pull(skb, count);
		skb_queue_head(rxq, skb);
	} else {
		dev_kfree_skb_any(skb);
	}

	return copied;
}

static const struct file_operations misc_io_fops = {
	.owner = THIS_MODULE,
	.open = misc_open,
	.release = misc_release,
	.poll = misc_poll,
	.unlocked_ioctl = misc_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = misc_compat_ioctl,
#endif
	.write = misc_write,
	.read = misc_read,
};

int exynos_init_gnss_io_device(struct io_device *iod, struct device *dev)
{
	int ret = 0;

	/* Matt - GNSS uses link headers; placeholder code */
	iod->link_header = true;

	/* Get data from link device */
	gif_info("%s: init\n", iod->name);
	iod->recv_skb = io_dev_recv_skb_from_link_dev;
	iod->recv_skb_single = io_dev_recv_skb_single_from_link_dev;

	/* Register misc device */
	init_waitqueue_head(&iod->wq);
	skb_queue_head_init(&iod->sk_rx_q);

	iod->miscdev.minor = MISC_DYNAMIC_MINOR;
	iod->miscdev.name = iod->name;
	iod->miscdev.fops = &misc_io_fops;
	iod->waketime = WAKE_TIME;
	iod->ws = gnssif_wake_lock_register(dev, iod->name);
	if (iod->ws == NULL) {
		gif_err("%s: wakeup_source_register fail\n", iod->name);
		return -EINVAL;
	}

	ret = misc_register(&iod->miscdev);
	if (ret)
		gif_err("%s: ERR! misc_register failed\n", iod->name);

	return ret;
}
