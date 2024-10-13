// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CHUB IPC Driver AP side
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *
 */

#include <linux/delay.h>
#include <linux/io.h>

#include "chub.h"
#include "ipc_chub.h"
#include "chub_log.h"
#define NAME_PREFIX "nanohub-ipc"

#define CSP_PRINTF_INFO(fmt, ...) nanohub_info(fmt, ##__VA_ARGS__)
#define CSP_PRINTF_ERROR(fmt, ...) nanohub_err(fmt, ##__VA_ARGS__)

u32 ipc_get_chub_mem_size(void)
{
	struct ipc_info *ipc = ipc_get_info();

	return ipc->ipc_addr[IPC_REG_DUMP].size;
}
EXPORT_SYMBOL(ipc_get_chub_mem_size);

void ipc_set_chub_clk(u32 clk)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->chubclk = clk;
}

u8 ipc_get_sensor_info(u8 type)
{
	int i;
	struct sensor_map *ipc_sensor_map =
	    ipc_get_base(IPC_REG_IPC_SENSORINFO);

	CSP_PRINTF_INFO("%s: senstype:%d, index:%d\n", __func__, type,
			ipc_sensor_map->index);
	if (ipc_have_sensor_info(ipc_sensor_map)) {
		for (i = 0; i < ipc_sensor_map->index; i++)
			if (ipc_sensor_map->sinfo[i].senstype == type) {
				CSP_PRINTF_INFO
				    ("%s: find: senstype:%d, id:%d\n", __func__,
				     type, ipc_sensor_map->sinfo[i].chipid);
				return ipc_sensor_map->sinfo[i].chipid;
			}
	}
	return 0;
}

void ipc_logbuf_flush_on(bool on)
{
	struct ipc_logbuf *logbuf;
	struct ipc_info *ipc = ipc_get_info();

	if (ipc->ipc_map) {
		logbuf = &ipc->ipc_map->logbuf;
		logbuf->flush_active = on;
	}
}

bool ipc_logbuf_filled(void)
{
	struct ipc_logbuf *logbuf;
	struct ipc_info *ipc = ipc_get_info();

	if (ipc->ipc_map) {
		logbuf = &ipc->ipc_map->logbuf;
		return logbuf->eq != logbuf->dq;
	}
	return 0;
}

#define LOGOUT_LOOP_CNT (100)
int ipc_logbuf_outprint(void *chub_p)
{
	struct ipc_info *ipc = ipc_get_info();
	struct contexthub_ipc_info *chub = chub_p;

	if (ipc->ipc_map) {
		struct logbuf_content *log;
		struct ipc_logbuf *logbuf = &ipc->ipc_map->logbuf;
		u32 eq;
		u32 len;
		u32 loop = LOGOUT_LOOP_CNT;

retry:
		eq = logbuf->eq;
		if (eq >= LOGBUF_NUM || logbuf->dq >= LOGBUF_NUM) {
			CSP_PRINTF_ERROR("%s: index  eq:%d, dq:%d\n", __func__, eq,
					 logbuf->dq);
			logbuf->eq = 0;
			logbuf->dq = 0;

			if (logbuf->eq != 0 || logbuf->dq != 0) {
				__raw_writel(0, &logbuf->eq);
				__raw_writel(0, &logbuf->dq);
				CSP_PRINTF_ERROR("%s: index after: eq:%d, dq:%d\n",
						 __func__, __raw_readl(&logbuf->eq),
						 __raw_readl(&logbuf->dq));
			}
			return -1;
		}

		if (logbuf->full) {
			logbuf->full = 0;
			logbuf->dq = (eq + 1) % LOGBUF_NUM;
		}

		while (eq != logbuf->dq) {
			if (atomic_read(&chub->chub_sleep)) {
				CSP_PRINTF_INFO("sram access after suspend\n");
				return 0;
			}
			log = &logbuf->log[logbuf->dq];
			len = strlen((char *)log);

			if (len > 0 && len <= LOGBUF_DATA_SIZE) {
				char buf[120];

				memset(buf, 0, 120);
				if (log->level || log->timestamp) {
					chub_printf(NULL, log->level, log->size,
						    "[%6llu.%06llu]%c %s",
						    (log->timestamp) / 1000000,
						    (log->timestamp) % 1000000,
						    log->level, (char *)log->buf);
				} else {
					chub_printf(NULL, 'I', log->size, "                 %s",
						    (char *)log->buf);
				}
			} else {
				CSP_PRINTF_ERROR("%s: %s: size err:%d, eq:%d, dq:%d\n",
						 NAME_PREFIX, __func__, len, eq,
						 logbuf->dq);
			}
			logbuf->dq = (logbuf->dq + 1) % LOGBUF_NUM;
		}
		msleep(20);
		if ((eq != logbuf->eq) && loop) {
			loop--;
			goto retry;
		}

		if (logbuf->flush_req)
			logbuf->flush_req = 0;
	}
	return 0;
}

void ipc_reset_map(void)
{
	struct ipc_info *ipc = ipc_get_info();

	ipc_hw_clear_all_int_pend_reg(ipc->mb_base, IPC_SRC_MB0);
	ipc_hw_clear_all_int_pend_reg(ipc->mb_base, IPC_DST_MB1);
	ipc_hw_set_mcuctrl(ipc->mb_base, 0x1);
	memset_io(ipc_get_base(IPC_REG_LOG), 0, ipc_get_size(IPC_REG_LOG));
	memset_io(cipc_get_base(CIPC_REG_AP2CHUB), 0, cipc_get_size(CIPC_REG_AP2CHUB));
	memset_io(cipc_get_base(CIPC_REG_CHUB2AP), 0, cipc_get_size(CIPC_REG_CHUB2AP));
	memset_io(cipc_get_base(CIPC_REG_ABOX2CHUB), 0, cipc_get_size(CIPC_REG_ABOX2CHUB));
	memset_io(cipc_get_base(CIPC_REG_CHUB2ABOX), 0, cipc_get_size(CIPC_REG_CHUB2ABOX));
	cipc_reset_map();
}
