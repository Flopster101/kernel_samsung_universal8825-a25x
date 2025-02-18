/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Common IPC Driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *
 */

#ifndef _COMMON_IPC_H
#define _COMMON_IPC_H

#include "ipc_hw.h"

#define IPC_BUF_NUM (IRQ_EVT_CH_MAX)
#define IPC_EVT_NUM (50)
#define CIPC_USER_DATA_MAX (2)
#define IPC_NAME_MAX (16)
#define CIPC_START_OFFSET (21 * 1024)
#define IPC_ALIGN_SIZE (4 * 1024)

struct ipc_area {
	char name[IPC_NAME_MAX];
	void *base;
	unsigned int size;
};

#undef CIPC_DEF_USER_ABOX
#undef CIPC_DEF_IPC_TEST_CHUB_ONLY_ABOX_TEST
#undef CIPC_DEF_USER_GNSS
#define CIPC_DEF_SUPPORT_BAAW_ALIGN

/* mailbox HW */
/* INTGR0(31~16) / INTGR1(15~0) */
/* INTCR0(31~16) / INTCR1(15~0) */
enum cipc_user_id {
	CIPC_USER_AP2CHUB,	// ap:src, chub:dst
	CIPC_USER_CHUB2AP,	// ap:src, chub:dst
#ifdef CIPC_DEF_USER_ABOX
	CIPC_USER_ABOX2CHUB,	// abox: src, chub:dst
	CIPC_USER_CHUB2ABOX,
#endif
#ifdef CIPC_DEF_USER_GNSS
	CIPC_USER_GNSS2CHUB,	//chub:src, ap:dst
	CIPC_USER_CHUB2GNSS,
#endif
	CIPC_USER_MAX,
};

enum cipc_region {
	CIPC_REG_SRAM_BASE,
	CIPC_REG_IPC_BASE,
	CIPC_REG_CIPC_BASE,
	/* common ipc for chub & ap */
	CIPC_REG_AP2CHUB,
	CIPC_REG_EVT_AP2CHUB,
	CIPC_REG_DATA_AP2CHUB,
	CIPC_REG_CHUB2AP,
	CIPC_REG_EVT_CHUB2AP,
	CIPC_REG_DATA_CHUB2AP,
	CIPC_REG_DATA_CHUB2AP_BATCH,
	/* common ipc for chub & abox */
	CIPC_REG_ABOX2CHUB, /* 10 */
	CIPC_REG_EVT_ABOX2CHUB,
	CIPC_REG_DATA_ABOX2CHUB_AUD,
	CIPC_REG_DATA_ABOX2CHUB, /* prox data */
	CIPC_REG_CHUB2ABOX,
	CIPC_REG_EVT_CHUB2ABOX,
	CIPC_REG_DATA_CHUB2ABOX, /* gyro data */
	/* common ipc for chub & gnss */
	CIPC_REG_GNSS2CHUB,
	CIPC_REG_EVT_GNSS2CHUB,
	CIPC_REG_DATA_GNSS2CHUB,

	CIPC_REG_CHUB2GNSS,
	CIPC_REG_EVT_CHUB2GNSS,
	CIPC_REG_DATA_CHUB2GNSS,
	CIPC_REG_MAX,
};

#define IPC_OWN_HOST (1)	/* owner of SRAM */
#define IPC_OWN_MASTER (2)	/* CIPC Map maker */

enum ipc_owner {
	IPC_OWN_CHUB = IPC_OWN_HOST,
	IPC_OWN_AP = IPC_OWN_MASTER,
	IPC_OWN_GNSS,
	IPC_OWN_ABOX,
	IPC_OWN_VTS,
	IPC_OWN_MAX,
};

enum ipc_data_id {
	CIPC_DATA_AP2CHUB,
	CIPC_DATA_CHUB2AP,
	CIPC_DATA_CHUB2AP_BATCH,
	CIPC_DATA_CHUB2ABOX,
	CIPC_DATA_ABOX2CHUB,
	CIPC_DATA_MAX,
};

enum cipc_irq {
	IRQ_NUM_EVT_START,
	IRQ_NUM_EVT_END = 14,
};

struct cipc_queue_ctrl {
	unsigned int eq;
	unsigned int dq;
	unsigned long long time;	/* for debug */
};

/* data structure */
struct cipc_data_ctrl {
	struct cipc_queue_ctrl qctrl;
	unsigned int size;	/* per channel size = <the user requested size> + sizeof(ipc_data_channel->size) */
	unsigned int cnt;	/* channel count */
};

struct cipc_data_channel {
	unsigned int size;
	char buf[0];
};

/* event structure */
struct cipc_evt_buf {
	unsigned int evt;
	unsigned int value;	/* cipc for value passing */
	unsigned int mb_irq;
	unsigned int status;
};

struct cipc_evt_ctrl {
	struct cipc_queue_ctrl qctrl;
	unsigned char pending[IRQ_HW_MAX];
};

struct cipc_trans_info {
	char name[IPC_NAME_MAX];
	unsigned int cnt;
	int err_cnt;
	unsigned int owner;
};

#define CIPC_MAGIC 		('C' << 0 | 'I' << 8 | 'P' << 16 | 'C' << 24)
#define CIPC_MAGIC_USER		('U' << 0 | 'S' << 8 | 'E' << 16 | 'R' << 24)
#define CIPC_MAGIC_USER_EVT	('E' << 0 | 'V' << 8 | 'T' << 16)
#define CIPC_MAGIC_USER_DATA	('D' << 0 | 'A' << 8 | 'T' << 16 | 'A' << 24)

struct cipc_evt {
	unsigned int magic;
	struct cipc_trans_info info;
	struct cipc_evt_ctrl ctrl;
	struct cipc_evt_buf data[IPC_EVT_NUM];
};

struct cipc_data {
	unsigned int magic;
	struct cipc_trans_info info;
	struct cipc_data_ctrl ctrl;
	char ch_pool[0];	/* struct ipc_data_channel *ch */
};

struct cipc_owner_info {
	unsigned int id;	/* enum ipc_owner id */
	unsigned int mb_id;	/* enum ipc_mb_id mb_id */
};

struct cipc_user {
	unsigned int magic;
	struct cipc_evt evt;
	char data_pool[0];
};

typedef void (*rx_isr_func) (int evt, void *priv);

struct cipc_user_map_info {
	char name[IPC_NAME_MAX];
	unsigned int reg;	/* enum cipc_region */
	unsigned int evt_reg;
	unsigned int data_reg[CIPC_USER_DATA_MAX];
	unsigned int data_size[CIPC_USER_DATA_MAX];
	unsigned int data_cnt[CIPC_USER_DATA_MAX];
	unsigned int owner;	/* enum ipc_onwer */
	struct cipc_owner_info src;
	struct cipc_owner_info dst;
	unsigned int data_pool_cnt;
};

struct cipc_user_info {		/* AP/ABOX/GNSS has 1, CHUB has 3 */
	struct cipc_user_map_info map_info;
	rx_isr_func rx_isr;
	void *priv;
	void *mb_base;
};

struct cipc_map_area {
	unsigned int magic;
	char user_pool[0];	/* struct cipc_user for cipc users */
};

/* Init IPC */
struct cipc_funcs {
	void (*mbusywait) (unsigned int ms);
	void (*memcpy) (void *dst, void *src, int size, int dst_io, int src_io);
	void (*memset) (void *buf, int val, int size, int io);
	void (*getlock_evt) (unsigned long *flag);
	void (*putlock_evt) (unsigned long *flag);
	void (*getlock_data) (unsigned long *flag);
	void (*putlock_data) (unsigned long *flag);
	int (*strncmp) (char *dst, char *src, int size);
	void (*strncpy) (char *dst, char *src, int size, int dst_io,
			 int src_io);
	void (*print) (const char *str, ...);
	void *priv;
};

/* CHUB Specific func: Read CHUB SRAM map and get IPC Offset */
#define MAP_INFO_OFFSET (0x200)
#define OS_UPDT_MAGIC	"Nanohub OS"
struct chub_bootargs_cipc {		/* struct chub_bootargs */
	char magic[16];
	unsigned int etc[5];	/* bl_start + bl_end + code_start + code_end + ipc_version */
	unsigned int ipc_start;
	unsigned int ipc_end;
	unsigned int cipc_init_map;
};

typedef void (*client_isr_func) (int evt, char *buf, unsigned int len);

struct cipc_info {
	struct ipc_area cipc_addr[CIPC_REG_MAX];
	struct chub_bootargs_cipc *chub_bootargs;
	struct cipc_map_area *cipc_map;
	void *sram_base;
	int lock;
	enum ipc_owner owner;
	struct cipc_funcs *user_func;
	int user_cnt;
	struct cipc_user_info user_info[CIPC_USER_MAX];	/* hold address of ipc_user */
	client_isr_func cb;
};

void cipc_register_callback(client_isr_func cb);
void *cipc_get_base(enum cipc_region area);
unsigned int cipc_get_size(enum cipc_region area);
int cipc_get_offset_owner(enum ipc_owner owner, unsigned int *start_adr,
			int *size);

struct cipc_info *cipc_get_info(void);
struct cipc_info *cipc_init(enum ipc_owner owner, void *sram_base,
			    struct cipc_funcs *funcs);
int cipc_reset_map(void);
int cipc_register(void *mb_addr, enum cipc_user_id tx, enum cipc_user_id rx,
		  rx_isr_func isr, void *priv, unsigned int *start_offset,
		  int *size);
/* Data IPC */
struct cipc_evt_buf *cipc_get_evt(enum cipc_region evtq);
int cipc_add_evt(enum cipc_region evtq, unsigned int evt);

/* Event IPC */
int cipc_write_data(enum cipc_region reg, void *tx, unsigned int length);
void *cipc_read_data(enum cipc_region reg, unsigned int *len);

int cipc_get_remain_qcnt(enum cipc_region reg);

/* debug */
int cipc_irqhandler(enum cipc_user_id id, unsigned int status);
void cipc_dump(enum cipc_user_id id);
void cipc_set_lock(int lock);

#undef IPC_DEF_IPC_TEST
#undef IPC_DEF_IPC_TEST_CB
#define CIPC_TEST_BAAW_REQ_BIT (16)
#ifdef IPC_DEF_IPC_TEST
#define CIPC_LOOPBACK_EVT (0xbb)
int cipc_loopback_test(int reg_val, int start);
#else
#define cipc_loopback_test(a, b) (void)(0)
#endif
#endif
