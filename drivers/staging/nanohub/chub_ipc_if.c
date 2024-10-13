// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CHUB IF Driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *      Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */

#include <linux/iio/iio.h>
#include "main.h"
#include "chub.h"
#include "chub_dbg.h"
#include "chub_exynos.h"
#if IS_ENABLED(CONFIG_SHUB)
#include "chub_shub.h"
#include <linux/platform_data/nanohub.h>
#endif
static DEFINE_MUTEX(wt_mutex);

/* CIPC Register */
static void cipc_func_sleep(unsigned int ms)
{
	msleep(ms);
}

static void cipc_func_memcpy(void *dst, void *src, int size, int dst_io,
			     int src_io)
{
	if (dst_io && src_io) {
		nanohub_info("%s: error: not support yet\n", __func__);
		return;
	}
	if (dst_io) {
		if ((u64)dst < 0xFFFF || (u64)src < 0xFFFF)
			nanohub_info("check dst/src state : dst : %p / src : %p\n", dst, src);
		memcpy_toio(dst, src, size);
		return;
	}
	if (src_io) {
		memcpy_fromio(dst, src, size);
		return;
	}
	memcpy(dst, src, size);
}

void cipc_func_memset(void *buf, int val, int size, int io)
{
	if (io)
		memset_io(buf, val, size);
	else
		memset(buf, val, size);
}

static DEFINE_SPINLOCK(ipc_evt_lock);
static DEFINE_SPINLOCK(ipc_data_lock);

static void cipc_func_get_evtlock(unsigned long *flag)
{
	spin_lock_irqsave(&ipc_evt_lock, *flag);
}

static void cipc_func_get_datalock(unsigned long *flag)
{
	spin_lock_irqsave(&ipc_data_lock, *flag);
}

static void cipc_func_put_evtlock(unsigned long *flag)
{
	spin_unlock_irqrestore(&ipc_evt_lock, *flag);
}

static void cipc_func_put_datalock(unsigned long *flag)
{
	spin_unlock_irqrestore(&ipc_data_lock, *flag);
}

static int cipc_func_strncmp(char *dst, char *src, int size)
{
	return strncmp(dst, src, size);
}

static void cipc_func_strncpy(char *dst, char *src, int size, int dst_io,
			      int src_io)
{
	if (!dst_io && !src_io)
		strncpy(dst, (const char *)src, (size_t)size);
	else
		cipc_func_memcpy(dst, src, size, dst_io, src_io);
}

static void cipc_func_logout(const char *str, ...)
{
	va_list vl;

	va_start(vl, str);
	printk(KERN_INFO, str, vl);
	va_end(vl);
}

static inline void contexthub_notify_host(struct contexthub_ipc_info *chub)
{
#ifdef CONFIG_SENSOR_DRV
	nanohub_handle_irq(chub->data);
#else
	/* TODO */
#endif
}

static void cipc_func_handle_irq(int evt, void *priv)
{
	int err;
	int dbg;
	int lock;
	struct contexthub_ipc_info *chub = priv;

	switch (evt) {
	case IRQ_EVT_C2A_DEBUG:
		if (contexthub_get_token(chub)) {
			nanohub_dev_warn(chub->dev, "%s: get token\n",
					 __func__);
			return;
		}
		dbg = ipc_read_value(IPC_VAL_C2A_DEBUG);
		if (dbg >= IPC_DEBUG_CHUB_FAULT) {
			err =
			    (dbg ==
			     IPC_DEBUG_CHUB_FAULT) ? CHUB_ERR_FW_FAULT :
			    CHUB_ERR_FW_ERROR;
			ipc_write_value(IPC_VAL_C2A_DEBUG, 0);
			contexthub_put_token(chub);
			nanohub_dev_err(chub->dev,
					"%s: c2a_debug: debug:%d, err:%d\n",
					__func__, dbg, err);
			contexthub_handle_debug(chub, err);
		} else {
			nanohub_dev_err(chub->dev, "%s: c2a_debug: debug:%d\n",
					__func__, dbg);
			ipc_write_value(IPC_VAL_C2A_DEBUG, 0);
		}
		break;
	case IRQ_EVT_C2A_INT:
		if (atomic_read(&chub->irq_apInt) == C2A_OFF) {
			atomic_set(&chub->irq_apInt, C2A_ON);
			contexthub_notify_host(chub);
		}
		break;
	case IRQ_EVT_C2A_INTCLR:
		atomic_set(&chub->irq_apInt, C2A_OFF);
		break;
	case CIPC_REG_DATA_CHUB2AP_BATCH:
		/* handle batch data */
		cipc_loopback_test(CIPC_REG_DATA_CHUB2AP_BATCH, 0);
		break;
#if IS_ENABLED(CONFIG_SHUB)
	case CIPC_REG_DATA_CHUB2AP:
		{
			int size = 0;
			char rx_buf[PACKET_SIZE_MAX] = {0,};
			void *raw_rx_buf = 0;
			struct contexthub_ipc_data packet;

			//pr_err("nanohub-ipc: ipc_read_data++");
			raw_rx_buf = cipc_read_data(CIPC_REG_DATA_CHUB2AP, &size);
			if (size > 0 && raw_rx_buf) {
				memcpy_fromio(rx_buf, (void *)raw_rx_buf, size);
				//pr_err("nanohub-ipc: ssp_handle_recv_packet++");
				packet.size = size;
				packet.data = rx_buf;
				contexthub_ipc_notifier_call(CHUB_IPC_CHUB2AP, &packet);
			}
			/* check : ipc_read err handle */
			break;
		}
#endif
#if IS_ENABLED(CONFIG_EXYNOS_AONCAM)
	case IRQ_EVT_C2A_FD_DETECT:
		aon_notifier_call(FD_DETECT, chub);
		break;
	case IRQ_EVT_A2C_AON_START:
		chub->AON_flag = AON_START_ACK;
		wake_up(&chub->aon_wait);
		break;
	case IRQ_EVT_A2C_AON_STOP:
		chub->aon_flag = AON_STOP_ACK;
		wake_up(&chub->aon_wait);
		break;
#endif
	default:
		atomic_inc(&chub->read_lock.cnt);
		/* TODO: requered.. ? */
		spin_lock(&chub->read_lock.event.lock);
		lock = atomic_read(&chub->read_lock.flag);
		spin_unlock(&chub->read_lock.event.lock);
		if (lock)
			wake_up_interruptible_sync(&chub->read_lock.event);
		break;
	};
}

struct cipc_funcs cipc_func = {
	.mbusywait = cipc_func_sleep,
	.memcpy = cipc_func_memcpy,
	.memset = cipc_func_memset,
	.getlock_evt = cipc_func_get_evtlock,
	.putlock_evt = cipc_func_put_evtlock,
	.getlock_data = cipc_func_get_datalock,
	.putlock_data = cipc_func_put_datalock,
	.strncmp = cipc_func_strncmp,
	.strncpy = cipc_func_strncpy,
	.print = cipc_func_logout,
};

#ifdef IPC_DEBUG
static void ipc_test(struct contexthub_ipc_info *chub)
{
	u64 test_val = 0xbb000bb00;
	int i;

	nanohub_info("%s: start: ipc write\n", __func__);
	for (i = 0; i < IPC_VAL_C2A_SENSORID; i++) {
		ipc_write_value(i, test_val);
		if (test_val != ipc_read_value(i))
			nanohub_info
			    ("%s: ipc_write_value(%d) fail: %llx <-> %llx\n",
			     __func__, i, test_val, ipc_read_value(i));
		else
			nanohub_info
			    ("%s: ipc_write_value(%d) pass: %llx <-> %llx\n",
			     __func__, i, test_val, ipc_read_value(i));
	}

	nanohub_info("%s: start: ipc hw write\n", __func__);
	test_val = 0xbb00;
	for (i = IPC_VAL_HW_BOOTMODE; i < IPC_VAL_HW_DEBUG; i++) {
		ipc_write_hw_value(i, test_val);
		if (test_val != ipc_read_hw_value(i))
			nanohub_info
			    ("%s: ipc_write_value(%d) fail: %llx <-> %llx\n",
			     __func__, i, test_val, ipc_read_hw_value(i));
		else
			nanohub_info
			    ("%s: ipc_write_value(%d) pass: %llx <-> %llx\n",
			     __func__, i, test_val, ipc_read_hw_value(i));
	}
}
#endif

#ifdef CONFIG_SENSOR_DRV
 /* by nanohub kernel RxBufStruct. packet header is 10 + 2 bytes to align */
struct rxbuf {
	u8 pad;
	u8 pre_preamble;
	u8 buf[PACKET_SIZE_MAX];
	u8 post_preamble;
};

static int nanohub_mailbox_open(void *data)
{
	return 0;
}

static void nanohub_mailbox_close(void *data)
{
	(void)data;
}

static int nanohub_mailbox_write(void *data, uint8_t *tx, int length,
				 int timeout)
{
	struct nanohub_data *chub = data;

	return contexthub_ipc_write(chub->pdata->mailbox_client, tx, length,
				    timeout);
}

static int nanohub_mailbox_read(void *data, uint8_t *rx, int max_length,
				int timeout)
{
	struct nanohub_data *chub = data;

	return contexthub_ipc_read(chub->pdata->mailbox_client, rx, max_length,
				   timeout);
}

static void nanohub_mailbox_comms_init(struct nanohub_comms *comms)
{
	comms->seq = 1;
	comms->timeout_write = 544;
	comms->timeout_ack = 272;
	comms->timeout_reply = 512;
	comms->open = nanohub_mailbox_open;
	comms->close = nanohub_mailbox_close;
	comms->write = nanohub_mailbox_write;
	comms->read = nanohub_mailbox_read;
}
#endif

#define os_name_idx (11)
int contexthub_get_sensortype(struct contexthub_ipc_info *chub, char *buf)
{
	struct sensor_map *sensor_map;
	struct sensor_map_pack *pack = (struct sensor_map_pack *)buf;
	int trycnt = 0;
	int ret;
	int i;

	if (atomic_read(&chub->chub_status) != CHUB_ST_RUN) {
		nanohub_dev_warn(chub->dev,
				 "%s :fails chub isn't active, status:%d, inreset:%d\n",
				 __func__, atomic_read(&chub->chub_status),
				 contexthub_read_in_reset(chub));
		contexthub_handle_debug(chub, CHUB_ERR_CHUB_ST_ERR);
		return -EINVAL;
	}

	do {
		ret = contexthub_get_token(chub);
		if (ret && (++trycnt < WAIT_TRY_CNT)) {
			nanohub_dev_warn(chub->dev,
					 "%s fails to get token: try:%d\n",
					 __func__, trycnt);
			msleep(WAIT_CHUB_MS);
		}
	} while (ret);

	if (ret) {
		nanohub_dev_warn(chub->dev, "%s fails to get token\n",
				 __func__);
		return -EINVAL;
	}

	sensor_map = ipc_get_base(IPC_REG_IPC_SENSORINFO);
	if (ipc_have_sensor_info(sensor_map)) {
		pack->num_os = os_image[chub->num_os][os_name_idx] - '0';
		memcpy(&pack->sensormap, sensor_map, sizeof(struct sensor_map));

		nanohub_dev_info(chub->dev,
				 "%s: get sensorinfo: (os:%d, size:%d, %d / %d %d), index:%d, map:%s\n",
				 __func__, pack->num_os,
				 ipc_get_size(IPC_REG_IPC_SENSORINFO),
				 sizeof(struct sensor_map_pack),
				 sizeof(pack->magic), sizeof(pack->num_os),
				 sensor_map->index, (char *)&pack->sensormap);

		for (i = 0; i < MAX_PHYSENSOR_NUM; i++)
			nanohub_dev_info(chub->dev, "%s2: %d/%d - %d: %s,%s\n",
					 __func__, i, pack->sensormap.index,
					 pack->sensormap.sinfo[i].sensortype,
					 pack->sensormap.sinfo[i].name,
					 pack->sensormap.sinfo[i].vendorname);
	}
	contexthub_put_token(chub);

	return sizeof(struct sensor_map_pack);
}

int contexthub_chub_ipc_init(struct contexthub_ipc_info *chub)
{
	u32 cipc_start_offset = 0;
	int cipc_size;
	int ret;

	nanohub_dev_info(chub->dev, "%s : chub_ipc\n", __func__);
	cipc_func.priv = chub;
	if (!chub->chub_ipc) {
		chub->chub_ipc =
		    ipc_init(IPC_OWN_AP, IPC_SRC_MB0, chub->sram, chub->mailbox,
			     &cipc_func);
		if (!chub->chub_ipc) {
			nanohub_dev_err(chub->dev, "%s: ipc_init failed\n",
					__func__);
			return -EINVAL;
		}

		nanohub_dev_info(chub->dev, "%s: ipc_init success\n", __func__);
		ret =
		    cipc_register(chub->mailbox, CIPC_USER_CHUB2AP,
				  CIPC_USER_AP2CHUB, cipc_func_handle_irq, chub,
				  &cipc_start_offset, &cipc_size);
		if (ret) {
			nanohub_dev_info(chub->dev,
					 "%s: c2a cipc_request fails\n",
					 __func__);
			return -EINVAL;
		}
		nanohub_dev_info(chub->dev,
				 "%s: cipc_register success: offset:+%x, size:%d\n",
				 __func__, cipc_start_offset, cipc_size);
	} else {
		nanohub_dev_info(chub->dev, "%s : with reset\n", __func__);
		ipc_reset_map();
	}
	return 0;
}

int contexthub_ipc_drv_init(struct contexthub_ipc_info *chub)
{
	int ret = 0;

	ret = contexthub_chub_ipc_init(chub);
	if (ret) {
		nanohub_err("%s: fails. ret:%d\n", __func__, ret);
	} else {
		ret = chub_dbg_init(chub, chub->chub_rt_log.buffer,
				    chub->chub_rt_log.buffer_size);
		if (ret)
			nanohub_err("%s: fails in chub_dbg_init. ret:%d\n",
				    __func__, ret);
	}
	return ret;
}

static bool contexthub_lowlevel_alive(struct contexthub_ipc_info *chub)
{
	int ret;
	struct ipc_info *chub_ipc = chub->chub_ipc;

	atomic_set(&chub->chub_alive_lock.flag, 0);
	ipc_hw_gen_interrupt(chub->mailbox, chub_ipc->opp_mb_id,
			     IRQ_NUM_CHUB_ALIVE);
	ret = chub_wait_event(&chub->chub_alive_lock, 200);
	nanohub_dev_info(chub->dev, "%s done: ret:%d\n", __func__, ret);
	return atomic_read(&chub->chub_alive_lock.flag);
}

int contexthub_ipc_write_event(struct contexthub_ipc_info *chub,
			       enum mailbox_event event)
{
	u32 val;
	int ret = 0;
	int need_ipc = 0;

	switch (event) {
	case MAILBOX_EVT_CHUB_ALIVE:
		nanohub_info("%s alive check\n", __func__);
		ipc_write_hw_value(IPC_VAL_HW_AP_STATUS, AP_WAKE);
		val = contexthub_lowlevel_alive(chub);
		if (val) {
			atomic_set(&chub->chub_status, CHUB_ST_RUN);
			nanohub_dev_info(chub->dev, "%s : chub is alive\n", __func__);
		} else if (!chub->multi_os) {
			nanohub_dev_err(chub->dev,
					"%s : chub isn't alive, status:%d, inreset:%d\n",
					__func__, atomic_read(&chub->chub_status),
					contexthub_read_in_reset(chub));
			ret = -EINVAL;
		}
		break;
	case MAILBOX_EVT_ENABLE_IRQ:
	case MAILBOX_EVT_DISABLE_IRQ:
		nanohub_dev_warn(chub->dev, "%s not support: %d\n", event);
		ret = -EINVAL;
		break;
	default:
		need_ipc = 1;
		break;
	}

	if (need_ipc) {
		if (atomic_read(&chub->chub_status) != CHUB_ST_RUN) {
			nanohub_dev_warn(chub->dev,
					 "%s event:%d/%d fails chub isn't active, status:%d, inreset:%d\n",
					 __func__, event, MAILBOX_EVT_MAX,
					 atomic_read(&chub->chub_status),
					 contexthub_read_in_reset(chub));
			contexthub_handle_debug(chub, CHUB_ERR_CHUB_ST_ERR);
			return -EINVAL;
		}
		clear_err_cnt(chub, CHUB_ERR_CHUB_ST_ERR);

		if (contexthub_get_token(chub))
			return -EINVAL;

		/* handle ipc */
		switch (event) {
		case MAILBOX_EVT_RT_LOGLEVEL:
			ipc_logbuf_loglevel(chub->chub_rt_log.loglevel, 1);
			break;
		case MAILBOX_EVT_DUMP_STATUS:
			/* dump nanohub kernel status */
			nanohub_dev_info(chub->dev,
					 "Request to dump chub fw status\n");
			ipc_write_value(IPC_VAL_A2C_DEBUG,
					MAILBOX_EVT_DUMP_STATUS);
			ret =
			    cipc_add_evt(CIPC_REG_EVT_AP2CHUB,
					 IRQ_EVT_A2C_DEBUG);
			break;
		case MAILBOX_EVT_WAKEUP_CLR:
			if (atomic_read(&chub->wakeup_chub) == CHUB_ON) {
				ret =
				    cipc_add_evt(CIPC_REG_EVT_AP2CHUB,
						 IRQ_EVT_A2C_WAKEUP_CLR);
				if (ret >= 0) {
					atomic_set(&chub->wakeup_chub,
						   CHUB_OFF);
					clear_err_cnt(chub,
						      CHUB_ERR_EVTQ_WAKEUP_CLR);
				} else {
					nanohub_dev_warn(chub->dev,
							 "%s: fails to set wakeup. ret:%d, err:%d",
							 __func__, ret,
							 chub->err_cnt[CHUB_ERR_EVTQ_WAKEUP_CLR]);
					contexthub_handle_debug(chub, CHUB_ERR_EVTQ_WAKEUP_CLR);
				}
			}
			break;
		case MAILBOX_EVT_WAKEUP:
			if (atomic_read(&chub->wakeup_chub) == CHUB_OFF) {
				ret =
				    cipc_add_evt(CIPC_REG_EVT_AP2CHUB,
						 IRQ_EVT_A2C_WAKEUP);
				if (ret >= 0) {
					atomic_set(&chub->wakeup_chub, CHUB_ON);
					clear_err_cnt(chub, CHUB_ERR_EVTQ_WAKEUP);
				} else {
					nanohub_dev_warn(chub->dev,
							 "%s: fails to set wakeupclr. ret:%d, err:%d",
							 __func__, ret,
							 chub->err_cnt[CHUB_ERR_EVTQ_WAKEUP]);
					contexthub_handle_debug(chub,
								CHUB_ERR_EVTQ_WAKEUP);
				}
			}
			break;
		case MAILBOX_EVT_DFS_GOVERNOR:
			ipc_write_value(IPC_VAL_A2C_DEBUG, (u32)event);
			ret = cipc_add_evt(CIPC_REG_EVT_AP2CHUB, IRQ_EVT_A2C_DEBUG);
			break;
		case MAILBOX_EVT_CLK_DIV:
			ipc_write_value(IPC_VAL_A2C_DEBUG, (u32)event);
			ret = cipc_add_evt(CIPC_REG_EVT_AP2CHUB, IRQ_EVT_A2C_DEBUG);
			break;
#if IS_ENABLED(CONFIG_EXYNOS_AONCAM)
		case MAILBOX_EVT_AON_START:
			ret = cipc_add_evt(CIPC_REG_EVT_AP2CHUB, IRQ_EVT_A2C_AON_START);
			break;
		case MAILBOX_EVT_AON_STOP:
			ret = cipc_add_evt(CIPC_REG_EVT_AP2CHUB, IRQ_EVT_A2C_AON_STOP);
			break;
#endif
		default:
			/* handle ipc utc */
			if ((int)event < IPC_DEBUG_UTC_MAX) {
				chub->utc_run = event;
				if ((int)event == IPC_DEBUG_UTC_TIME_SYNC)
					contexthub_check_time();
				ipc_write_value(IPC_VAL_A2C_DEBUG, (u32)event);
				ret = cipc_add_evt(CIPC_REG_EVT_AP2CHUB, IRQ_EVT_A2C_DEBUG);
			}
			break;
		}
		contexthub_put_token(chub);

		if (ret < 0) {
			contexthub_handle_debug(chub, CHUB_ERR_EVTQ_ADD);
			nanohub_dev_warn(chub->dev,
					 "%s: errcnt: evtq:%d, wake:%d, clr:%d\n",
					 __func__,
					 chub->err_cnt[CHUB_ERR_EVTQ_ADD],
					 chub->err_cnt[CHUB_ERR_EVTQ_WAKEUP],
					 chub->err_cnt[CHUB_ERR_EVTQ_WAKEUP_CLR]);
		} else {
			clear_err_cnt(chub, CHUB_ERR_EVTQ_ADD);
		}
	}
	return ret;
}

static int contexthub_read_process(uint8_t *rx, u8 *raw_rx, u32 size)
{
#ifdef CONFIG_SENSOR_DRV
	struct rxbuf *rxstruct;
	struct nanohub_packet *packet;

	rxstruct = (struct rxbuf *)raw_rx;
	packet = (struct nanohub_packet *)&rxstruct->pre_preamble;
	memcpy_fromio(rx, (void *)packet, size);

	return NANOHUB_PACKET_SIZE(packet->len);
#else
	memcpy_fromio(rx, (void *)raw_rx, size);
	return size;
#endif
}

int contexthub_ipc_write(struct contexthub_ipc_info *chub,
			 uint8_t *tx, int length, int timeout)
{
	int ret;

	if (__raw_readl(&chub->chub_status) != CHUB_ST_RUN) {
		nanohub_dev_warn(chub->dev, "%s: chub isn't run:%d\n",
				 __func__, __raw_readl(&chub->chub_status));
		contexthub_handle_debug(chub, CHUB_ERR_CHUB_ST_ERR);
		return 0;
	}
	clear_err_cnt(chub, CHUB_ERR_CHUB_ST_ERR);

	if (contexthub_get_token(chub)) {
		nanohub_dev_warn(chub->dev, "no-active: write fails\n");
		return 0;
	}

	mutex_lock(&wt_mutex);
	ret = cipc_write_data(CIPC_REG_DATA_AP2CHUB, tx, length);
	mutex_unlock(&wt_mutex);
	contexthub_put_token(chub);
	if (ret) {
		nanohub_err
		    ("%s: fails to write data: ret:%d, len:%d errcnt:%d\n",
		     __func__, ret, length, chub->err_cnt[CHUB_ERR_WRITE_FAIL]);
		contexthub_handle_debug(chub, CHUB_ERR_WRITE_FAIL);
		length = 0;
	} else {
		clear_err_cnt(chub, CHUB_ERR_WRITE_FAIL);
	}
	return length;
}
EXPORT_SYMBOL(contexthub_ipc_write);

int contexthub_ipc_read(struct contexthub_ipc_info *chub, uint8_t *rx,
			int max_length, int timeout)
{
	unsigned long flag;
	int size = 0;
	int ret = 0;
	void *rxbuf;
	u64 time = 0;		/* for debug */
	bool wait_event_err = false;
	bool restart_err = false;

	if (__raw_readl(&chub->chub_status) != CHUB_ST_RUN) {
		nanohub_dev_warn(chub->dev, "%s: chub isn't run:%d\n",
				 __func__, __raw_readl(&chub->chub_status));
		contexthub_handle_debug(chub, CHUB_ERR_CHUB_ST_ERR);
		return 0;
	}
	clear_err_cnt(chub, CHUB_ERR_CHUB_ST_ERR);

	if (!atomic_read(&chub->read_lock.cnt)) {
		time = sched_clock();

		spin_lock_irqsave(&chub->read_lock.event.lock, flag);
		atomic_inc(&chub->read_lock.flag);
		ret =
		    wait_event_interruptible_timeout_locked(chub->read_lock.event,
							    atomic_read(&chub->read_lock.cnt),
							    msecs_to_jiffies(timeout));
		atomic_dec(&chub->read_lock.flag);
		spin_unlock_irqrestore(&chub->read_lock.event.lock, flag);
		if (ret < 0) {
			wait_event_err = true;
			if (ret == (-ERESTARTSYS))
				restart_err = true;
			nanohub_dev_warn(chub->dev,
					 "fails to get read ret:%d timeout:%d, err:%d\n",
					 ret, timeout, restart_err);
		}
	}

	if (contexthub_get_token(chub)) {
		nanohub_dev_warn(chub->dev, "no-active: read fails\n");
		return 0;
	}

	if (atomic_read(&chub->read_lock.cnt)) {
		rxbuf = cipc_read_data(CIPC_REG_DATA_CHUB2AP, &size);
		if (size > 0)
			ret = contexthub_read_process(rx, rxbuf, size);
		atomic_dec(&chub->read_lock.cnt);
	} else {
		if (chub->err_cnt[CHUB_ERR_READ_FAIL])
			nanohub_dev_info(chub->dev,
					 "%s: read timeout(%d): c2aq_cnt:%d, recv_cnt:%d during %lld ns\n",
					 __func__,
					 chub->err_cnt[CHUB_ERR_READ_FAIL],
					 cipc_get_remain_qcnt
					 (CIPC_REG_DATA_CHUB2AP),
					 atomic_read(&chub->read_lock.cnt),
					 sched_clock() - time);
		if (cipc_get_remain_qcnt(CIPC_REG_DATA_CHUB2AP)) {
			rxbuf = cipc_read_data(CIPC_REG_DATA_CHUB2AP, &size);
			if (size > 0)
				ret = contexthub_read_process(rx, rxbuf, size);
		} else {
			ret = -EINVAL;
		}
	}

	if (wait_event_err)	/* add debug log for wait_event_err */
		nanohub_dev_info(chub->dev,
				 "%s: read in wait event err, ret:%d\n", __func__, ret);

	if (ret < 0) {
		if (chub->err_cnt[CHUB_ERR_READ_FAIL]) {
			nanohub_err
			    ("%s: fails to read data: ret:%d, len:%d errcnt:%d, err:%d\n",
			     __func__, ret, chub->err_cnt[CHUB_ERR_READ_FAIL],
			     restart_err);
			ipc_dump();
			if (!restart_err)
				contexthub_handle_debug(chub, CHUB_ERR_READ_FAIL);
		} else {
			chub->err_cnt[CHUB_ERR_READ_FAIL]++;
		}
	} else {
		clear_err_cnt(chub, CHUB_ERR_READ_FAIL);
	}
	contexthub_put_token(chub);
	return ret;
}

int contexthub_ipc_if_init(struct contexthub_ipc_info *chub)
{
#ifdef CONFIG_SENSOR_DRV
	struct iio_dev *iio_dev;

	/* nanohub probe */
	iio_dev = nanohub_probe(chub->dev, NULL);
	if (IS_ERR(iio_dev))
		return -EINVAL;

	/* set wakeup irq number on nanohub driver */
	chub->data = iio_priv(iio_dev);
	nanohub_mailbox_comms_init(&chub->data->comms);
	chub->pdata = chub->data->pdata;
	chub->pdata->mailbox_client = chub;
	chub->data->irq = chub->irq_mailbox;
#endif

	init_waitqueue_head(&chub->read_lock.event);
	return 0;
}
