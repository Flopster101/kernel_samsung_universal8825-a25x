// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Common IPC Driver Mailbox HW Control
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *      Boojin Kim <boojin.kim@samsung.com>
 *
 */

#include "ipc_hw.h"
#include "chub_exynos.h"

int ipc_hw_start_bit(enum ipc_mb_id which_mb)
{
	return (which_mb == IPC_SRC_MB0) ? MB0_BIT_OFFSET : 0;
}

void ipc_hw_set_mcuctrl(void *base, unsigned int val)
{
	CIPC_RAW_WRITEL(val, (char *)(base + REG_MAILBOX_MCUCTL));
}

void ipc_hw_write_shared_reg(void *base, unsigned int val, int num)
{
	CIPC_RAW_WRITEL(val, (char *)(base + REG_MAILBOX_ISSR0 + num * 4));
}

unsigned int ipc_hw_read_shared_reg(void *base, int num)
{
	return CIPC_RAW_READL((char *)(base + REG_MAILBOX_ISSR0 + num * 4));
}

/* SRC: GR0:Get interrupt , SR0:Interrupt Status, CR0:Pending Clear : Bit:31~16 */
/* DST: GR1:Get Interrupt, SR1, CR1 Bit:15~0 */
unsigned int ipc_hw_read_int_status_reg(void *base, enum ipc_mb_id which_mb,
					int irq)
{
	unsigned int val = 1 << (irq + ipc_hw_start_bit(which_mb));
	unsigned int offset =
	    (which_mb == IPC_SRC_MB0) ? REG_MAILBOX_INTSR0 : REG_MAILBOX_INTSR1;

	return CIPC_RAW_READL((char *)(base + offset)) & val;
}

unsigned int ipc_hw_read_int_status_reg_all(void *base, enum ipc_mb_id which_mb)
{
	unsigned int offset =
	    (which_mb == IPC_SRC_MB0) ? REG_MAILBOX_INTSR0 : REG_MAILBOX_INTSR1;

	return CIPC_RAW_READL((char *)(base + offset));
}

void ipc_hw_clear_int_pend_reg(void *base, enum ipc_mb_id which_mb, int irq)
{
	unsigned int offset =
	    (which_mb == IPC_SRC_MB0) ? REG_MAILBOX_INTCR0 : REG_MAILBOX_INTCR1;

	CIPC_RAW_WRITEL(1 << irq, (char *)(base + offset));
}

void ipc_hw_clear_all_int_pend_reg(void *base, enum ipc_mb_id which_mb)
{
	unsigned int offset =
	    (which_mb == IPC_SRC_MB0) ? REG_MAILBOX_INTCR0 : REG_MAILBOX_INTCR1;
	unsigned int val =
	    (which_mb == IPC_SRC_MB0) ? (0xffff << MB0_BIT_OFFSET) : 0xffff;

	CIPC_RAW_WRITEL(val, (char *)(base + offset));
}

void ipc_hw_gen_interrupt(void *base, enum ipc_mb_id which_mb, int irq)
{
	unsigned int offset =
	    (which_mb == IPC_SRC_MB0) ? REG_MAILBOX_INTGR0 : REG_MAILBOX_INTGR1;
	unsigned int val =
	    (which_mb ==
	     IPC_SRC_MB0) ? (1 << (irq + MB0_BIT_OFFSET)) : (1 << irq);

	CIPC_RAW_WRITEL(val, (char *)(base + offset));
}

unsigned int ipc_hw_read_int_gen_reg(void *base, enum ipc_mb_id which_mb)
{
	unsigned int offset =
	    (which_mb == IPC_SRC_MB0) ? REG_MAILBOX_INTGR0 : REG_MAILBOX_INTGR1;

	return CIPC_RAW_READL((char *)(base + offset));
}
