#ifndef __LINUX_PLATFORM_DATA_NANOHUB_H
#define __LINUX_PLATFORM_DATA_NANOHUB_H

#include <linux/types.h>

struct nanohub_flash_bank {
	int bank;
	u32 address;
	size_t length;
};

#define MAX_FILE_LEN (32)

struct nanohub_platform_data {
	void *mailbox_client;
	int irq;
};

enum CHUB_STATE {
	CHUB_FW_ST_INVAL,
	CHUB_FW_ST_POWERON,
	CHUB_FW_ST_OFF,
	CHUB_FW_ST_ON,
};

/* Structure of notifier block */
struct contexthub_notifier_block {
	const char *subsystem;
	unsigned int start_off;
	unsigned int end_off;
	enum CHUB_STATE state;
	int (*notifier_call)(struct contexthub_notifier_block *);
};

int contexthub_notifier_register(struct contexthub_notifier_block *nb);

#if IS_ENABLED(CONFIG_SHUB)
enum CHUB_DUMP_TYPE {
	CHUB_DUMPSTATE,
	CHUB_ERR_DUMP
};

enum CHUB_IPC_TYPE {
	CHUB_IPC_CHUB2AP
};

struct contexthub_dump {
	int reason;
	int size;
	char *dump;
};

struct contexthub_ipc_data {
	int size;
	void *data;
};

int contexthub_ipc_notifier_register(struct notifier_block *nb);
int contexthub_dump_notifier_register(struct notifier_block *nb);
#endif
#endif /* __LINUX_PLATFORM_DATA_NANOHUB_H */
