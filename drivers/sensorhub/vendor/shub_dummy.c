#include "../comm/shub_comm.h"
#include "../debug/shub_dump.h"
#include "../sensorhub/shub_device.h"
#include "../utility/shub_utility.h"

void shub_handle_recv_packet(char *packet, int packet_len)
{
}

void shub_dump_write_file(void *dump_data, int dump_size, int err_type)
{
}

int sensorhub_probe(void)
{
	return 0;
}

int sensorhub_comms_write(u8 *buf, int length)
{
	return 0;
}

int sensorhub_reset(void)
{
	return 0;
}

void sensorhub_save_ram_dump(void)
{
}

bool sensorhub_is_working(void)
{
	return 1;
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
}

int sensorhub_get_dump_size(void)
{
	return 0;
}
