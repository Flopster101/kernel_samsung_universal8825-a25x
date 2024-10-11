//#include "../../staging/nanohub/chub.h"
//#include "../../staging/nanohub/chub_dbg.h"

#include "../comm/shub_comm.h"
#include "../debug/shub_dump.h"
#include "../sensorhub/shub_device.h"
#include "../sensorhub/shub_firmware.h"
#include "../utility/shub_utility.h"

#include <linux/platform_data/nanohub.h>

struct contexthub_notifier_block chub_state_nb;
struct notifier_block chub_ipc_nb;
struct notifier_block chub_dump_nb;

static atomic_t chub_state;

extern int contexthub_reset_call(void);
extern int contexthub_poweron_call(void);
extern int contexthub_ipc_write_call(uint8_t *tx, int length, int timeout);
extern void get_chub_current_sram_dump(void);
extern u32 ipc_get_chub_mem_size(void);

void shub_handle_recv_packet(char *packet, int packet_len)
{
	handle_packet(packet, packet_len);
}

void shub_dump_write_file(void *dump_data, int dump_size, int err_type)
{
	struct shub_data_t *data = get_shub_data();
	int dump_type;

	if (err_type == 0 && data->reset_type < RESET_TYPE_MAX)
		dump_type = DUMP_TYPE_BASE + data->reset_type;
	else
		dump_type = err_type;

	write_shub_dump_file((char *)dump_data, dump_size, dump_type, 1);
}

/*
	this is called when chub state is chagned if state != CHUB_FW_ST_INVAL
*/
static int contexthub_notifier(struct contexthub_notifier_block *nb)
{

	atomic_set(&chub_state, nb->state);
	shub_infof("chub st %d", (int)nb->state);

	if (nb->state == CHUB_FW_ST_POWERON) { // after power on
		queue_refresh_task();
	} else if (nb->state == CHUB_FW_ST_OFF) { // start reset
		sensorhub_stop();
	} else if (nb->state == CHUB_FW_ST_ON) { // end reset
		queue_refresh_task();
	}
	return 0;
}

static int contexthub_ipc_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct contexthub_ipc_data *ipc_data = (struct contexthub_ipc_data *)data;

	shub_handle_recv_packet(ipc_data->data, ipc_data->size);
	return 0;
}

static int contexthub_dump_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct contexthub_dump *dump_data = (struct contexthub_dump *)data;

	shub_dump_write_file(dump_data->dump, dump_data->size, dump_data->reason);
	return 0;
}

int sensorhub_probe(void)
{
	atomic_set(&chub_state, CHUB_FW_ST_INVAL);

	/* chub power & reset notifier */
	chub_state_nb.subsystem = "CHUB";
	chub_state_nb.notifier_call = contexthub_notifier;
	contexthub_notifier_register(&chub_state_nb);

	chub_ipc_nb.notifier_call = contexthub_ipc_notifier;
	contexthub_ipc_notifier_register(&chub_ipc_nb);

	chub_dump_nb.notifier_call = contexthub_dump_notifier;
	contexthub_dump_notifier_register(&chub_dump_nb);

	enable_sensor_vdd();

	return 0;
}

int sensorhub_comms_write(u8 *buf, int length)
{
	int ret;

	ret = contexthub_ipc_write_call(buf, length, 0);
	if (ret == 0) {
		shub_errf("fail %d", ret);
		ret = -EIO;
	}
	return ret;
}

int sensorhub_reset(void)
{
	int ret;

	if (atomic_read(&chub_state) == CHUB_FW_ST_INVAL) { /* chub is no power status*/
		ret = contexthub_poweron_call();
	} else
		ret = contexthub_reset_call();
	return ret;
}

void sensorhub_save_ram_dump(void)
{
	get_chub_current_sram_dump();
}

bool sensorhub_is_working(void)
{
	if (atomic_read(&chub_state) == CHUB_FW_ST_POWERON || atomic_read(&chub_state) == CHUB_FW_ST_ON)
		return true;
	else
		return false;
}

int sensorhub_refresh_func(void)
{
	return 0;
}

int sensorhub_shutdown(void)
{
	return 0;
}

void sensorhub_fs_ready(void)
{
	if (contexthub_poweron_call() < 0)
		shub_errf("contexthub power on failed");
	else {
		chub_state_nb.state = CHUB_FW_ST_POWERON;
		atomic_set(&chub_state, CHUB_FW_ST_POWERON);
		queue_refresh_task();
	}
}

int sensorhub_get_dump_size(void)
{
	return ipc_get_chub_mem_size();
}