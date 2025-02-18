/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../utility/shub_utility.h"
#include "../debug/shub_dump.h"
#include "shub_mtk.h"

#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include "../../misc/mediatek/scp/rv/scp_helper.h"
#include "../../misc/mediatek/scp/rv/scp_scpctl.h"
#endif

#define IPI_SHUB	    IPI_SENSOR
#define IPI_DMA_LEN 8

static DEFINE_SPINLOCK(scp_state_lock);
static uint8_t scp_system_ready;

static phys_addr_t shub_dram_phys;
static phys_addr_t shub_dram_virt;
static phys_addr_t shub_dram_size;
#if !defined(CONFIG_MTK_TINYSYS_SCP_RV_SUPPORT) && (LINUX_VERSION_CODE <= KERNEL_VERSION(5, 0, 0))
void scp_wdt_reset(enum scp_core_id cpu_id);
#endif

static void sensorhub_IPI_handler(int id, void *packet, unsigned int len)
{
	struct shub_data_t *data = get_shub_data();

	if (!data->is_probe_done) {
		shub_infof("shub is not working");
		return;
	}

	if (id == IPI_SHUB) {
		if (len == IPI_DMA_LEN) {
			uint32_t dma_info[2] = {0, }; /* address, size */

			memcpy(dma_info, packet, len);
			handle_packet((void *)(shub_dram_virt + dma_info[0]), dma_info[1]);
		} else {
			handle_packet(packet, len);
		}
	}
}

static int notify_scp_state(struct notifier_block *this, unsigned long event, void *ptr)
{
	unsigned long flags = 0;

	shub_infof("notify event %d", event);

	if (event == SCP_EVENT_STOP) { // 1
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, false);
		spin_unlock_irqrestore(&scp_state_lock, flags);
		sensorhub_stop();
	}

	if (event == SCP_EVENT_READY) { // 0
		spin_lock_irqsave(&scp_state_lock, flags);
		WRITE_ONCE(scp_system_ready, true);
		spin_unlock_irqrestore(&scp_state_lock, flags);
		queue_refresh_task();
	}

	return NOTIFY_DONE;
}

static struct notifier_block scp_state_notifier = {
	.notifier_call = notify_scp_state,
};

void shub_dump_write_file(void *dump_data, int dump_size)
{
	struct shub_data_t *data = get_shub_data();
	int dump_type;

	if (!data) {
		shub_infof("shub is not probed yet !");
		return;
	}

	if (data->reset_type < RESET_TYPE_MAX)
		dump_type = DUMP_TYPE_BASE + data->reset_type;
	else {
		shub_errf("hub crash");
		dump_type = 1;
		data->hub_crash_timestamp = get_current_timestamp();
	}

	write_shub_dump_file((char *)dump_data, dump_size, dump_type, 4);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static int shub_dump_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct shub_data_t *shub_data = get_shub_data();
	struct shub_dump *dump_data = (struct shub_dump *)data;

	shub_infof("ram_dump : %x mini_dump : %x", dump_data->dump, dump_data->mini_dump);
	shub_dump_write_file(dump_data->dump, dump_data->size);
	memcpy(shub_data->mini_dump, dump_data->mini_dump, MINI_DUMP_LENGTH);
	return 0;
}

struct notifier_block shub_dump_nb = {
	.notifier_call = shub_dump_notifier,
};
#endif

int sensorhub_probe(void)
{
	scp_ipi_registration(IPI_SHUB, sensorhub_IPI_handler, "shub_sensorhub");
	scp_A_register_notify(&scp_state_notifier);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
	shub_dump_notifier_register(&shub_dump_nb);
#endif
	return 0;
}

int sensorhub_shutdown(void)
{
	scp_A_unregister_notify(&scp_state_notifier);
	return 0;
}

int sensorhub_refresh_func(void)
{
	int ret;

	int dram_info[2];
	shub_dram_phys = scp_get_reserve_mem_phys(SENS_MEM_ID);
	shub_dram_virt = scp_get_reserve_mem_virt(SENS_MEM_ID);
	shub_dram_size = scp_get_reserve_mem_size(SENS_MEM_ID);

	dram_info[0] = shub_dram_phys;
	dram_info[1] = shub_dram_size;

	shub_infof("dma address phys 0x%llx virt 0x%llx size 0x%llx", shub_dram_phys, shub_dram_virt, shub_dram_size);
	// ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, DMA_ADDRESS, (char *)&shub_dram_phys, sizeof(shub_dram_phys));
	ret = shub_send_command(CMD_SETVALUE, TYPE_HUB, DMA_ADDRESS, (char *)dram_info, sizeof(dram_info));

	if (ret < 0)
		shub_errf("DMA_ADDRESS CMD fail %d", ret);

	return 0;
}

int sensorhub_comms_write(u8 *buf, int length)
{
	int status = 0, retry = 0;

	do {
		status = scp_ipi_send(IPI_SHUB, (unsigned char *)buf, length, 0, SCP_A_ID);
		if (status == SCP_IPI_ERROR) {
			shub_errf("scp_ipi_send fail");
			return -1;
		}
		if (status == SCP_IPI_BUSY) {
			if (retry++ == 1000) {
				shub_errf("retry fail");
				return -1;
			}
			if (retry % 100 == 0)
				usleep_range(1000, 2000);
		}
	} while (status == SCP_IPI_BUSY);

	return status;
}

bool sensorhub_is_working(void)
{
	struct shub_data_t *data = get_shub_data();

	return	READ_ONCE(scp_system_ready) && data->is_probe_done && !work_busy(&data->work_reset);
}

int sensorhub_reset(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	int ret;
	struct scpctl_cmd_s cmd;

	cmd.type = SCPCTL_DEBUG_LOGIN;
	cmd.op = SCP_DEBUG_MAGIC_PATTERN;

	ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_SCPCTL_1, 0, &cmd, PIN_OUT_SIZE_SCPCTL_1, 0);

	if (ret != IPI_ACTION_DONE)
		pr_notice("sending ipi failed, %d\n", ret);

	mdelay(10);
#endif
	scp_wdt_reset(SCP_A_ID);
	return 0;
}

void sensorhub_fs_ready(void)
{
}

void sensorhub_save_ram_dump(void)
{
}

int sensorhub_get_dump_size(void)
{
	int dump_size = get_scp_dump_size();

	shub_infof("scp dump size : %d", dump_size);

	return dump_size;
}
