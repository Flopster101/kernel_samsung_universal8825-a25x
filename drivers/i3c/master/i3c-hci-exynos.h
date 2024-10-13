// SPDX-License-Identifier: GPL-2.0-only
/*
 * i3c-hci-exynos.h - Samsung Exynos I3C HCI Controller Driver
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; You can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#define MAX_DEVS 8

struct exynos_i3c_hci_master_caps {
	u32 cmdfifodepth;
	u32 respfifodepth;
	u32 txfifodepth;
	u32 rxfifodepth;
	u32 ibiqfifodepth;
	u32 ibidfifodepth;
};

struct exynos_i3c_hci_master {
	struct work_struct hj_work;
	struct device	*dev;
	struct i3c_dev_desc *desc;
	struct list_head node;
	struct i3c_master_controller base;
	u32 free_rr_slots;
	unsigned int maxdevs;
	unsigned long i3c_scl_lim;

	struct {
		unsigned int num_slots;
		struct i3c_dev_desc **slots;
		spinlock_t lock;
	} ibi;
	struct {
		struct list_head list;
		struct exynos_i3c_hci_xfer *xfer;
		spinlock_t lock;
	} xfer_queue;
	void __iomem *regs;
	struct clk *ipclk;
	struct clk *gate_clk;
	struct exynos_i3c_hci_master_caps caps;
	unsigned int suspended;
	u32 current_master;
	u8 dat_idx;
	u8 addrs[MAX_DEVS];

	u32 DAT[MAX_DEVS];
	u32 EXT_DAT[MAX_DEVS];
	u32 DCT[MAX_DEVS][4];

	// dt property

	unsigned int hold_initiating_command;
	int nack_retry_cnt;
	unsigned int dma_mode;
	unsigned int hot_join_nack;
	unsigned int i3c_bus_mode;
	unsigned int notify_sir_rejected;
	unsigned int notify_mr_rejected;
	unsigned int notify_hj_rejected;
	unsigned int mr_req_nack;
	unsigned int sir_req_nack;
	unsigned int filter_en_sda;
	unsigned int filter_en_scl;
	unsigned int fs_clock;
	unsigned int fs_plus_clock;
	unsigned int open_drain_clock;
	unsigned int push_pull_clock;

	u8 i2c_slv_present;
	u8 iba_include;

	u8 hwacg_enable_s;
	u8 notify_vgpio_rx;
	u8 vgpio_en;
	u8 slave_mode;
	u8 data_swap;
	u8 getmxds_5byte;
	u8 tx_dma_free_run;
	u8 rx_stall_en;
	u8 tx_stall_en;
	u8 addr_opt_en;
};

struct exynos_i3c_hci_cmd {
	u32 cmd;
	u32 tx_len;
	const void *tx_buf;
	u32 rx_len;
	void *rx_buf;
	u32 error;
};

struct exynos_i3c_hci_xfer {
	struct list_head node;
	struct completion comp;
	int ret;
	unsigned int ncmds;
	struct exynos_i3c_hci_cmd cmds[0];
};

struct exynos_i3c_hci_i2c_dev_data {
	u16 id;
	s16 ibi;
	struct i3c_generic_ibi_pool *ibi_pool;
};

