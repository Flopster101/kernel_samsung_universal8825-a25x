/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */
#ifndef _S5E9925_H_
#define _S5E9925_H_

#undef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX			(4096)
#define CHUB_CLK_BASE			393216000

#define CONTEXTHUB_UPMU
/* uPMU SFRs */
#define SEL_UPMU			BIT(13)
#define UPMU_SYSTEM_CTRL		0x0
#define RESETN_HCU			BIT(1)
#define UPMU_INT_EN			0x144
#define UPMU_INT_TYPE			0x148
#define WAKEUP_REQ			BIT(1)
#define PD_REQ				BIT(0)

#define REG_CHUB_CPU_STATUS			(0x0)
#define REG_CHUB_CPU_OPTION			(0x0)
#define ENABLE_SYSRESETREQ			BIT(0)
#define CHUB_RESET_RELEASE_VALUE		(0x0)

#define REG_MAILBOX_ISSR0 (0x100)
#define REG_MAILBOX_ISSR1 (0x104)
#define REG_MAILBOX_ISSR2 (0x108)
#define REG_MAILBOX_ISSR3 (0x10c)

/*aon*/
enum aon_status {
	AON_WAIT_ACK,
	AON_START_ACK,
	AON_STOP_ACK,
};
#endif /* _S5E9925_H_ */
