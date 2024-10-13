/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PERI_MAILBOX_H_
#define PERI_MAILBOX_H_

#include <linux/version.h>
#include "npu-config.h"

#define MAILBOX_SIGNATURE1		0x0C0FFEE0
#define MAILBOX_SIGNATURE2		0xC0DEC0DE
#define MAILBOX_SHARED_MAX		64

#define MAILBOX_H2FCTRL_LPRIORITY	0
#define MAILBOX_H2FCTRL_HPRIORITY	1
#define MAILBOX_H2FCTRL_MAX		2

#define MAILBOX_F2HCTRL_RESPONSE	0
#define MAILBOX_F2HCTRL_REPORT		1
#define MAILBOX_F2HCTRL_MAX		2

#define MAILBOX_GROUP_LPRIORITY	0
#define MAILBOX_GROUP_HPRIORITY	2
#define MAILBOX_GROUP_RESPONSE		3
#define MAILBOX_GROUP_REPORT		4
#define MAILBOX_GROUP_MAX		5

#define MAX_MAILBOX	4

enum numCtrl_e {
	ECTRL_LOW = 1,
	ECTRL_HIGH,
	ECTRL_ACK,
	ECTRL_REPORT,
};

enum npu_mbox_index_e {
	NPU_MBOX_REQUEST_LOW = 0,
	NPU_MBOX_REQUEST_HIGH,
	NPU_MBOX_RESULT,//and RESPONSE
	NPU_MBOX_LOG,
	NPU_MBOX_RESERVED,
	NPU_MBOX_MAX_ID
};

/*
 *  +-----------------------------+ ------------
 *  |      Mailbox #4             |          |
 *  +-----------------------------+          |
 *  |      Mailbox #3             |          |
 *  |                             |          |
 *  +-----------------------------+          |
 *  |      Mailbox #2             |         16K
 *  |                             |      (= NPU_MAILBOX_SIZE)
 *  +-----------------------------+          |
 *  |      Mailbox #1             |          |
 *  |                             |          |
 *  +-----------------------------+  ----    |
 *  |       Padding               |   |      |
 *  +-----------------------------+   4K     |
 *  |    struct mailbox_hdr       |   |      |
 *  +-----------------------------+  ----------- <- npu_mailbox->mbox_base by Eung ju kim
 */

/* Mailbox HW */
/* group0 : low priority
 * group1 : reserved for score
 * group2 : high priority
 * group3 : response
 * group4 : log
 */
struct intr_group {
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
	u32 g; /* Interrupt Generation Register */
	u32 c; /* Interrupt Clear Register */
	u32 m; /* Interrupt Mask register */
	u32 s; /* Interrupt Status Register */
	u32 ms; /* Interrupt Mask Status Register */
#else
	u32 st;	/* status */
	u32 ms;	/* mask status */
	u32 e;	/* enable */
	u32 s;	/* set */
	u32 c;	/* clear */
#endif
};

struct mailbox_sfr {
#ifdef CONFIG_NPU_USE_MAILBOX_GROUP
	u32			mcuctl;
	u32			pad0;
	struct intr_group	grp[MAILBOX_GROUP_MAX];
	u32			pad1;
	u32			version;
	u32			pad2[3];
	u32			shared[MAILBOX_SHARED_MAX];
#else
	/* 0th group sends irq; 1st group receives irq
	*  each group has 15 lines to send interrupt
	*/
	struct intr_group       grp[2];
#endif
};

struct mailbox_ctrl {
	u32			sgmt_ofs; /* minus offset from SRAM END(0x80000) */
	u32			sgmt_len; /* plus length in byte from sgmt_ofs */
	u32			wptr;
	u32			rptr;
};

struct mailbox_hdr {
	u32			max_slot; /* the maximum number of ncp object slot */
	u32			debug_time; /* start time in micro second at boot */
	u32			debug_code; /* firmware boot progress */
	u32			log_level; /* this means the time when read trigger is generated on report mailbox */
	u32			warm_boot_enable;
	u32			reserved[8];
	struct mailbox_ctrl	h2fctrl[MAILBOX_H2FCTRL_MAX];
	struct mailbox_ctrl	f2hctrl[MAILBOX_F2HCTRL_MAX];
	u32			totsize;
	u32			version;	/* half low 16 bits is mailbox ipc version, half high 16 bits is command version */
	u32			signature2; /* host should update this after host configuration is done */
	u32			signature1; /* firmware should update this after firmware initialization is done */
};

struct MAILBOX {
	volatile struct mailbox_sfr *sfr;
	struct uk_event *event[MAILBOX_H2FCTRL_MAX];
};

//void mailbox_init(u32 addr);
void mailbox_send_interrupt(u32 idx);

static u32 NPU_MAILBOX_SECTION_CONFIG[MAX_MAILBOX] = {
	/* TODO: increase the size */
	1 * K_SIZE,         /* Size of 1st mailbox */
	1 * K_SIZE,         /* Size of 2nd mailbox */
	2 * K_SIZE,         /* Size of 3rd mailbox */
	8 * K_SIZE,         /* Size of 4th mailbox */
};

#define NPU_MBOX_BASE(x)		((x)+(sizeof(struct mailbox_hdr)))
#define NPU_MAILBOX_GET_CTRL(x, y)	((x)-(y))

#endif	/* PERI_MAILBOX_H_ */
