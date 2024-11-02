#include "chub.h"
#include "ipc_chub.h"

#include <linux/firmware.h>
#include <linux/spu-verify.h>
#include <linux/platform_data/nanohub.h>

#define SPU_FW_FILE "spu/sensorhub/shub_spu.bin"
#define FW_VER_LEN  8

struct contexthub_ipc_info *chub_info;
struct raw_notifier_head ipc_notifier_chain;
struct raw_notifier_head dump_notifier_chain;
bool use_spu_firmware;

void set_contexthub_info(struct contexthub_ipc_info *chub)
{
	chub_info = chub;
}

void contexthub_dump_notifier_call(int val, struct contexthub_dump *dump)
{
	raw_notifier_call_chain(&dump_notifier_chain, val, dump);
}

void contexthub_ipc_notifier_call(int val, struct contexthub_ipc_data *data)
{
	raw_notifier_call_chain(&ipc_notifier_chain, val, data);
}

int contexthub_ipc_notifier_register(struct notifier_block *nb)
{
	int ret;

	ret = raw_notifier_chain_register(&ipc_notifier_chain, nb);

	return ret;
}
EXPORT_SYMBOL(contexthub_ipc_notifier_register);

int contexthub_dump_notifier_register(struct notifier_block *nb)
{
	int ret;

	ret = raw_notifier_chain_register(&dump_notifier_chain, nb);

	return ret;
}
EXPORT_SYMBOL(contexthub_dump_notifier_register);

int contexthub_poweron_call(void)
{
	return contexthub_poweron(chub_info);
}
EXPORT_SYMBOL(contexthub_poweron_call);

int contexthub_reset_call(bool force_load)
{
	return contexthub_reset(chub_info, true, CHUB_ERR_NONE);
}
EXPORT_SYMBOL(contexthub_reset_call);

int contexthub_ipc_write_call(uint8_t *tx, int length, int timeout)
{
	return contexthub_ipc_write(chub_info, tx, length, timeout);
}
EXPORT_SYMBOL(contexthub_ipc_write_call);

void get_chub_current_sram_dump(void)
{
	struct contexthub_dump dump;

	dump.reason = 0;
	dump.dump = ipc_get_base(IPC_REG_DUMP);
	dump.size = ipc_get_chub_mem_size();

	contexthub_dump_notifier_call(CHUB_DUMPSTATE, &dump);
}
EXPORT_SYMBOL(get_chub_current_sram_dump);


