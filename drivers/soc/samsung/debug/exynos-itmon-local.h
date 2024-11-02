/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver for Samsung Exynos2100 SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>
#include <soc/samsung/exynos-itmon.h>
#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-adv-tracer-ipc.h>

/* #define MULTI_IRQ_SUPPORT_ITMON */

#define OFFSET_TMOUT_REG		(0x2000)
#define OFFSET_REQ_R			(0x0)
#define OFFSET_REQ_W			(0x20)
#define OFFSET_RESP_R			(0x40)
#define OFFSET_RESP_W			(0x60)
#define OFFSET_ERR_REPT			(0x20)
#define OFFSET_PRT_CHK			(0x100)
#define OFFSET_NUM			(0x4)

#define REG_INT_MASK			(0x0)
#define REG_INT_CLR			(0x4)
#define REG_INT_INFO			(0x8)
#define REG_EXT_INFO_0			(0x10)
#define REG_EXT_INFO_1			(0x14)
#define REG_EXT_INFO_2			(0x18)
#define REG_EXT_USER			(0x80)

#define REG_DBG_CTL			(0x10)
#define REG_TMOUT_INIT_VAL		(0x14)
#define REG_TMOUT_FRZ_EN		(0x18)
#define REG_TMOUT_FRZ_STATUS		(0x1C)
#define REG_TMOUT_BUF_WR_OFFSET		(0x20)

#define REG_TMOUT_BUF_POINT_ADDR	(0x20)
#define REG_TMOUT_BUF_ID		(0x24)

#define REG_TMOUT_BUF_PAYLOAD_0		(0x28)
#define REG_TMOUT_BUF_PAYLOAD_1		(0x2C)
#define REG_TMOUT_BUF_PAYLOAD_2		(0x30)
#define REG_TMOUT_BUF_PAYLOAD_3		(0x34)
#define REG_TMOUT_BUF_PAYLOAD_4		(0x38)
#define REG_TMOUT_BUF_PAYLOAD_5		(0x3C)

#define REG_PRT_CHK_CTL		(0x4)
#define REG_PRT_CHK_INT		(0x8)
#define REG_PRT_CHK_INT_ID		(0xC)
#define REG_PRT_CHK_START_ADDR_LOW	(0x10)
#define REG_PRT_CHK_END_ADDR_LOW	(0x14)
#define REG_PRT_CHK_START_END_ADDR_UPPER	(0x18)

#define RD_RESP_INT_ENABLE		(1 << 0)
#define WR_RESP_INT_ENABLE		(1 << 1)
#define ARLEN_RLAST_INT_ENABLE		(1 << 2)
#define AWLEN_WLAST_INT_ENABLE		(1 << 3)
#define INTEND_ACCESS_INT_ENABLE	(1 << 4)

#define BIT_PRT_CHK_ERR_OCCURRED(x)	(((x) & (0x1 << 0)) >> 0)
#define BIT_PRT_CHK_ERR_CODE(x)	(((x) & (0x7 << 1)) >> 1)

#define BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_AXID(x)			(((x) & (0xFFFF)))
#define BIT_AXUSER(x)			(((x) & (0xFFFFFFFF)))
#define BIT_AXUSER_PERI(x)		(((x) & (0xFFFF << 16)) >> 16)
#define BIT_AXBURST(x)			(((x) & (0x3)))
#define BIT_AXPROT(x)			(((x) & (0x3 << 2)) >> 2)
#define BIT_AXLEN(x)			(((x) & (0xF << 16)) >> 16)
#define BIT_AXSIZE(x)			(((x) & (0x7 << 28)) >> 28)

#define ERRCODE_SLVERR			(0)
#define ERRCODE_DECERR			(1)
#define ERRCODE_UNSUPPORTED		(2)
#define ERRCODE_POWER_DOWN		(3)
#define ERRCODE_UNKNOWN_4		(4)
#define ERRCODE_UNKNOWN_5		(5)
#define ERRCODE_TMOUT			(6)

#define DATA_FROM_EXT			(0)
#define PERI				(1)
#define BUS_PATH_TYPE			(2)
#define DATA_FROM_INFO2			(3)

#define TRANS_TYPE_WRITE		(0)
#define TRANS_TYPE_READ			(1)
#define TRANS_TYPE_NUM			(2)

#define CP_COMMON_STR			"MODEM_"
#define NO_NAME				"N/A"

#define TMOUT_VAL			(0xFFFFF)
#define TMOUT_TEST			(0x1)

#define ERR_THRESHOLD			(5)

/* This value will be fixed */
#define INTEND_ADDR_START		(0)
#define INTEND_ADDR_END			(0)

#define GET_IRQ(x)			(x & 0xFFFF)
#define GET_AFFINITY(x)			((x >> 16) & 0xFFFF)

#define SET_IRQ(x)			(x & 0xFFFF)
#define SET_AFFINITY(x)			((x & 0xFFFF) << 16)

#define OFFSET_DSS_CUSTOM		(0xF000)
#define CUSTOM_MAX_PRIO			(7)

enum err_type {
	TMOUT,
	PRTCHKER,
	DECERR,
	SLVERR,
	FATAL,
	TYPE_MAX,
};

struct itmon_policy {
	char *name;
	int policy_def;
	int policy;
	int prio;
	bool error;
};

struct itmon_rpathinfo {
	unsigned short id;
	char port_name[16];
	char dest_name[16];
	unsigned short bits;
} __packed;

struct itmon_masterinfo {
	char port_name[16];
	unsigned short user;
	char master_name[24];
	unsigned short bits;
} __packed;

struct itmon_nodegroup;

struct itmon_traceinfo {
	char *port;
	char *master;
	char *dest;
	u64 target_addr;
	u32 errcode;
	bool read;
	bool onoff;
	bool path_dirty;
	bool dirty;
	u32 path_type;
	char buf[SZ_32];
	u32 axsize;
	u32 axlen;
	u32 axburst;
	u32 axprot;
	bool baaw_prot;
	struct itmon_nodeinfo *m_node;
	struct itmon_nodeinfo *s_node;
	struct list_head list;
};

struct itmon_tracedata {
	u32 int_info;
	u32 ext_info_0;
	u32 ext_info_1;
	u32 ext_info_2;
	u32 ext_user;
	u32 dbg_mo_cnt;
	u32 prt_chk_ctl;
	u32 prt_chk_info;
	u32 prt_chk_int_id;
	u32 offset;
	struct itmon_traceinfo *ref_info;
	struct itmon_nodegroup *group;
	struct itmon_nodeinfo *node;
	bool logging;
	bool read;
	struct list_head list;
};

struct itmon_nodepolicy {
	union {
		u64 enabled;
		struct {
			u64 chk_set : 1;
			u64 prio : 3;
			u64 chk_errrpt : 1;
			u64 en_errrpt : 1;
			u64 chk_tmout : 1;
			u64 en_tmout : 1;
			u64 chk_prtchk : 1;
			u64 en_prtchk : 1;
			u64 chk_tmout_val : 1;
			u64 en_tmout_val : 28;
			u64 chk_freeze : 1;
			u64 en_freeze : 1;
			u64 chk_irq_mask : 1;
			u64 en_irq_mask : 1;
			u64 chk_job : 1;
			u64 chk_decerr_job : 1;
			u64 en_decerr_job : 3;
			u64 chk_slverr_job : 1;
			u64 en_slverr_job : 3;
			u64 chk_tmout_job : 1;
			u64 en_tmout_job : 3;
			u64 chk_prtchk_job : 1;
			u64 en_prtchk_job : 3;
		};
	};
} __packed;

struct itmon_nodeinfo {
	char name[16];
	unsigned short type : 8;
	unsigned short err_enabled : 1;
	unsigned short prt_chk_enabled : 1;
	u32  addr_detect_enabled : 1;
	u32 retention : 1;
	u32 tmout_enabled : 1;
	u32 tmout_frz_enabled : 1;
	u32 time_val : 28;
	u64 phy_regs;
	void __iomem *regs;
	struct itmon_nodegroup *group;
	struct itmon_nodepolicy policy;
} __packed;

struct itmon_nodegroup {
	char name[16];
	u64 phy_regs;
	bool ex_table;
	struct itmon_nodeinfo *nodeinfo_phy;
	struct itmon_nodeinfo *nodeinfo;
	u32 nodesize;
	u32 path_type;
	void __iomem *regs;
	u32 irq;
	bool pd_support;
	bool pd_status;
	char pd_name[16];
	u32 src_in;
} __packed;

struct itmon_keepdata {
	u32 magic;
	u64 base;
	u64 mem_rpathinfo;
	u32 size_rpathinfo;
	u32 num_rpathinfo;
	u64 mem_masterinfo;
	u32 size_masterinfo;
	u32 num_masterinfo;
	u64 mem_nodegroup;
	u32 size_nodegroup;
	u32 num_nodegroup;
} __packed;

struct itmon_platdata {
	struct itmon_rpathinfo *rpathinfo;
	struct itmon_masterinfo *masterinfo;
	struct itmon_nodegroup *nodegroup;

	int num_rpathinfo;
	int num_masterinfo;
	int num_nodegroup;

	struct list_head infolist[TRANS_TYPE_NUM];
	struct list_head datalist[TRANS_TYPE_NUM];
	unsigned long last_time;
	int last_errcnt;

	struct itmon_policy *policy;
	bool cpu_parsing;
	bool irq_oring;
	bool big_bus;
	bool probed;
	struct adv_tracer_plugin *itmon_adv;
	bool adv_enabled;
	u32 sysfs_tmout_val;
	bool enabled;
};

struct itmon_dev {
	struct device *dev;
	struct itmon_platdata *pdata;
	struct itmon_keepdata *kdata;
	struct of_device_id *match;
	u32 irq;
	int id;
	void __iomem *regs;
	spinlock_t ctrl_lock;
	struct itmon_notifier notifier_info;
};

struct itmon_panic_block {
	struct notifier_block nb_panic_block;
	struct itmon_dev *pdev;
};
