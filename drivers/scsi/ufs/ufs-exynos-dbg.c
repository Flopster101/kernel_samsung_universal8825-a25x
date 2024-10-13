/*
 * UFS debugging functions for Exynos specific extensions
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/module.h>

#include "ufshcd.h"
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/memlogger.h>
#include "ufs-vs-mmio.h"
#include "ufs-vs-regs.h"
#include "ufs-dump.h"

struct exynos_ufs_memlog {
	struct memlog *desc;
	struct memlog_obj *log_obj_file;
	struct memlog_obj *log_obj;
	unsigned int log_enable;
};

/* Structure for ufs cmd logging */
#define MAX_CMD_LOGS    128
/* Hardware */
#define UIC_ARG_MIB_SEL(attr, sel)		((((attr) & 0xFFFF) << 16) |\
						((sel) & 0xFFFF))
#define UIC_ARG_MIB(attr)			UIC_ARG_MIB_SEL(attr, 0)
#define PCS_CMN_OFFSET(ofs)			(0x2800 + ((ofs) - 0x200) * 4)
#define PCS_TRSV_OFFSET(ofs)			(0x2000 + (ofs) * 4)
#define PHY_PMA_COMN_ADDR(reg)			(reg)
#define PHY_PMA_TRSV_ADDR(reg, lane)		((reg) + (0x400 * (lane)))
#define UNIP_COMP_AXI_AUX_FIELD			0x040
#define __WSTRB					(0xF << 24)
#define __SEL_IDX(L)				((L) & 0xFFFF)
enum {
	TX_LANE_0 = 0,
	RX_LANE_0 = 4,
};

struct cmd_data {
	unsigned int tag;
	unsigned int sct;
	unsigned long lba;
	u64 start_time;
	u64 end_time;
	u64 outstanding_reqs;
	int retries;
	unsigned char op;
};

struct ufs_cmd_info {
	u32 total;
	u32 last;
	struct cmd_data data[MAX_CMD_LOGS];
	struct cmd_data *pdata[32];	/* Currently, 32 slots */
};

static const char *ufs_sfr_field_name[3] = {
	"NAME",	"OFFSET", "VALUE",
};
static const char *ufs_attr_field_name[4]= {
	"MIB",	"OFFSET(SFR)", "VALUE(lane #0)", "VALUE(lane #1)",
};

#define ATTR_LANE_OFFSET		16
#define ATTR_TYPE_MASK(addr)		((addr >> ATTR_LANE_OFFSET) & 0xF)
#define ATTR_SET(addr, type)		((addr) | ((type & 0xF) << ATTR_LANE_OFFSET))

#define ATTR_NUM_MAX_LANES		2

#define CPORT_TX_BUF_SIZE       0x100
#define CPORT_RX_BUF_SIZE       0x100
#define CPORT_LOG_PTR_SIZE      0x100
#define CPORT_UTRL_SIZE         0x400
#define CPORT_UCD_SIZE          0x400
#define CPORT_UTMRL_SIZE        0xc0
#define CPORT_BUF_SIZE          (CPORT_TX_BUF_SIZE + CPORT_RX_BUF_SIZE + \
                                CPORT_LOG_PTR_SIZE +  CPORT_UTRL_SIZE + \
                                CPORT_UCD_SIZE +  CPORT_UTMRL_SIZE)

#define CPORT_TX_BUF_PTR        0x0
#define CPORT_RX_BUF_PTR        (CPORT_TX_BUF_PTR + CPORT_TX_BUF_SIZE)
#define CPORT_LOG_PTR_OFFSET    (CPORT_RX_BUF_PTR + CPORT_RX_BUF_SIZE)
#define CPORT_UTRL_PTR          (CPORT_LOG_PTR_OFFSET + CPORT_LOG_PTR_SIZE)
#define CPORT_UCD_PTR           (CPORT_UTRL_PTR + CPORT_UTRL_SIZE)
#define CPORT_UTMRL_PTR         (CPORT_UCD_PTR + CPORT_UCD_SIZE)

#define pr_memlog(dev, fmt, ...)					\
	do {								\
		if (dev->log_enable)					\
			memlog_write_printf(dev->log_obj,		\
					MEMLOG_LEVEL_EMERG,		\
					fmt, ##__VA_ARGS__);		\
		else							\
			pr_err(fmt, ##__VA_ARGS__);			\
	} while (0)

struct ufs_log_cport {
        u32 ptr;
        u8 buf[CPORT_BUF_SIZE];
} ufs_log_cport;

#define DBG_NUM_OF_HOSTS	1
struct ufs_dbg_mgr {
	struct ufs_vs_handle *handle;
	int active;
	u64 first_time;
	u64 time;
	u32 lanes;

	/* cport */
	struct ufs_log_cport log_cport;

	/* cmd log */
	struct ufs_cmd_info cmd_info;
	struct cmd_data cmd_log;		/* temp buffer to put */
	spinlock_t cmd_lock;

	/* mem log*/
	struct exynos_ufs_memlog *mem_log;
	struct ufs_stats ufs_stats;
};
static struct ufs_dbg_mgr ufs_dbg[DBG_NUM_OF_HOSTS];
static struct exynos_ufs_memlog ufs_memlog[DBG_NUM_OF_HOSTS];
static int ufs_dbg_mgr_idx = 0;

static void __ufs_get_sfr(struct ufs_dbg_mgr *mgr,
					struct exynos_ufs_sfr_log* cfg)
{
	struct ufs_vs_handle *handle = mgr->handle;
	int sel_api = 0;
	u32 reg = 0;
	u32 *pval;

	while(cfg) {
		if (!cfg->name)
			break;

		if (cfg->offset >= LOG_STD_HCI_SFR) {
			/* Select an API to get SFRs */
			sel_api = cfg->offset;
			if (sel_api == LOG_PMA_SFR) {
				/* Enable MPHY APB */
				reg = hci_readl(handle, HCI_CLKSTOP_CTRL);
				hci_writel(handle, reg & ~MPHY_APBCLK_STOP, HCI_CLKSTOP_CTRL);
			}
			cfg++;
			continue;
		}

		/* Fetch value */
		pval = &cfg->val[SFR_VAL_H_0];
		if (sel_api == LOG_STD_HCI_SFR)
			*pval = std_readl(handle, cfg->offset);
		else if (sel_api == LOG_VS_HCI_SFR)
			*pval = hci_readl(handle, cfg->offset);
		else if (sel_api == LOG_UNIPRO_SFR)
			*pval = unipro_readl(handle, cfg->offset);
		else if (sel_api == LOG_PMA_SFR)
			*pval = pma_readl(handle, cfg->offset);
		else
			*pval = 0xFFFFFFFF;

		/* Keep the first contexts permanently */
		if (mgr->first_time == 0ULL)
			cfg->val[SFR_VAL_H_0_FIRST] = *pval;

		/* Next SFR */
		cfg++;
	}

	/* Disable MPHY APB */
	hci_writel(handle, reg | MPHY_APBCLK_STOP, HCI_CLKSTOP_CTRL);
}

static u32 __read_pcs(struct ufs_vs_handle *handle, int lane,
		      struct exynos_ufs_attr_log* cfg) {
	unipro_writel(handle, __WSTRB | __SEL_IDX(lane),
		      UNIP_COMP_AXI_AUX_FIELD);
	return unipro_readl(handle, PCS_TRSV_OFFSET(cfg->mib));
}
static void __ufs_get_attr(struct ufs_dbg_mgr *mgr,
					struct exynos_ufs_attr_log* cfg)
{
	struct ufs_vs_handle *handle = mgr->handle;
	int sel_api = 0;
	u32 val;
	u32 *pval;
	int i;

	while(cfg) {
		if (cfg->mib == 0)
			break;

		/* Fetch result and value */
		pval = &cfg->val[ATTR_VAL_H_0_L_0];
		if (cfg->mib >= DBG_ATTR_UNIPRO) {
			/* Select an API to get attributes */
			sel_api = cfg->mib;
			cfg++;
			continue;
		}

		cfg->val[ATTR_VAL_H_0_L_1] = 0xFFFFFFFF;
		if (sel_api == DBG_ATTR_UNIPRO) {
			*pval = unipro_readl(handle, cfg->offset);
		} else if (sel_api == DBG_ATTR_PCS_CMN) {
			*pval = unipro_readl(handle, PCS_CMN_OFFSET(cfg->mib));
		} else if (sel_api == DBG_ATTR_PCS_TX) {
			for (i = 0 ; i < mgr->lanes; i++) {
				val = __read_pcs(handle, TX_LANE_0 + i, cfg);
				*(pval + i) = val;
			}
		} else if (sel_api == DBG_ATTR_PCS_RX) {
			for (i = 0 ; i < mgr->lanes; i++) {
				val = __read_pcs(handle, RX_LANE_0 + i, cfg);
				*(pval + i) = val;
			}
		} else
			// TODO:
			;

		/* Keep the first contexts permanently */
		if (mgr->first_time == 0ULL) {
			cfg->val[ATTR_VAL_H_0_L_0_FIRST] = *pval;
			cfg->val[ATTR_VAL_H_0_L_1_FIRST] = *(pval + 1);
		}

		/* Next attribute */
		cfg++;
	}
}

static void __ufs_print_sfr(struct ufs_vs_handle *handle,
					struct device *dev,
					struct exynos_ufs_sfr_log* cfg)
{

	struct ufs_dbg_mgr *mgr = (struct ufs_dbg_mgr *)handle->private;

	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
	pr_memlog(mgr->mem_log, ":\t\tREGISTER\n");
	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
	pr_memlog(mgr->mem_log, ": %-30s\t%-012s\t%-014s\n",
				ufs_sfr_field_name[0],
				ufs_sfr_field_name[1],
				ufs_sfr_field_name[2]);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ":\t\tREGISTER\n");
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ": %-30s\t%-012s\t%-014s\n",
		ufs_sfr_field_name[0],
		ufs_sfr_field_name[1],
		ufs_sfr_field_name[2]);
#endif

	while(cfg) {
		if (!cfg->name)
			break;

		/* show */
		if (cfg->offset >= LOG_STD_HCI_SFR)
			pr_memlog(mgr->mem_log, "\n");
		pr_memlog(mgr->mem_log, ": %-30s\t0x%-012x\t0x%-014x\n",
				cfg->name, cfg->offset, cfg->val[SFR_VAL_H_0]);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		dev_err(dev, ": %-30s\t0x%-012x\t0x%-014x\n",
			cfg->name, cfg->offset, cfg->val[SFR_VAL_H_0]);
#endif
		if (cfg->offset >= LOG_STD_HCI_SFR)
			pr_memlog(mgr->mem_log, "\n");

		/* Next SFR */
		cfg++;
	}
}

static void __ufs_print_attr(struct ufs_vs_handle *handle,
					struct device *dev,
					struct exynos_ufs_attr_log* cfg)
{
	struct ufs_dbg_mgr *mgr = (struct ufs_dbg_mgr *)handle->private;

	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
	pr_memlog(mgr->mem_log, ":\t\tATTRIBUTE\n");
	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
	pr_memlog(mgr->mem_log, ": %-30s\t%-12s%-14s\t%-14s\n",
				ufs_attr_field_name[0],
				ufs_attr_field_name[1],
				ufs_attr_field_name[2],
				ufs_attr_field_name[3]);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ":\t\tATTRIBUTE\n");
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ": %-30s\t%-12s%-14s\t%-14s\n",
		ufs_attr_field_name[0],
		ufs_attr_field_name[1],
		ufs_attr_field_name[2],
		ufs_attr_field_name[3]);
#endif

	while(cfg) {
		if (!cfg->mib)
			break;

		/* show */
		if (cfg->offset >= DBG_ATTR_UNIPRO)
			pr_memlog(mgr->mem_log, "\n");
		pr_memlog(mgr->mem_log, ": 0x%-27x\t0x%-012x\t0x%-014x\t0x%-014x\n",
				cfg->mib, cfg->offset,
				cfg->val[ATTR_VAL_H_0_L_0], cfg->val[ATTR_VAL_H_0_L_1]);
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		dev_err(dev, ": 0x%-27x\t0x%-012x\t0x%-014x\t0x%-014x\n",
			cfg->mib, cfg->offset,
			cfg->val[ATTR_VAL_H_0_L_0], cfg->val[ATTR_VAL_H_0_L_1]);
#endif
		if (cfg->offset >= DBG_ATTR_UNIPRO)
			pr_memlog(mgr->mem_log, "\n");

		/* Next SFR */
		cfg++;
	}
}

static void __print_cport(struct device *dev, int tag_printed,
						u32 tag, u32 *ptr,
						struct ufs_dbg_mgr *mgr)
{
	if (tag_printed)
		pr_memlog(mgr->mem_log, "%08x %08x %08x %08x tag# %u\n",
			be32_to_cpu(*(ptr + 0)), be32_to_cpu(*(ptr + 1)),
			be32_to_cpu(*(ptr + 2)), be32_to_cpu(*(ptr + 3)),
			tag);
	else
		pr_memlog(mgr->mem_log, "%08x %08x %08x %08x\n",
			be32_to_cpu(*(ptr + 0)), be32_to_cpu(*(ptr + 1)),
			be32_to_cpu(*(ptr + 2)), be32_to_cpu(*(ptr + 3)));
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
	if (tag_printed)
		dev_err(dev, "%08x %08x %08x %08x tag# %u\n",
			be32_to_cpu(*(ptr + 0)), be32_to_cpu(*(ptr + 1)),
			be32_to_cpu(*(ptr + 2)), be32_to_cpu(*(ptr + 3)),
			tag);
	else
		dev_err(dev, "%08x %08x %08x %08x\n",
			be32_to_cpu(*(ptr + 0)), be32_to_cpu(*(ptr + 1)),
			be32_to_cpu(*(ptr + 2)), be32_to_cpu(*(ptr + 3)));
#endif
}

static void __ufs_print_cport(struct ufs_dbg_mgr *mgr, struct device *dev)
{
	struct ufs_vs_handle *handle = mgr->handle;
	struct ufs_log_cport *log_cport = &mgr->log_cport;
	u32 *buf_ptr;
	u8 *buf_ptr_out;
	u32 offset;
	u32 size = 0;
	u32 cur_ptr = 0;
	u32 tag = 0;
	u32 idx = 0;
	int tag_printed;

	/*
	 * CPort logger
	 *
	 * [ log type 0 ]
	 * First 4 double words
	 *
	 * [ log type 1 ]
	 * 6 double words, for Rx
	 * 8 double words, for Tx
	 *
	 * [ log type 2 ]
	 * 4 double words, for Command UPIU, DATA OUT/IN UPIU and RTT.
	 * 2 double words, otherwise.
	 *
	 */
	log_cport->ptr = cport_readl(handle, CPORT_LOG_PTR_OFFSET);
	buf_ptr = (u32 *)&log_cport->buf[0];
	size = 0;
	offset = 0;

	while (size < CPORT_BUF_SIZE) {
		*buf_ptr = cport_readl(handle, offset);
		size += 4;
		buf_ptr += 1;
		offset += 4;
	}

	/* memory barrier for ufs cport dump */
	mb();


	/* Print data */
	buf_ptr_out = &log_cport->buf[0];
	cur_ptr = 0;

	pr_memlog(mgr->mem_log, "cport logging finished\n");
	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
	pr_memlog(mgr->mem_log, ":\t\tCPORT\n");
	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
	dev_err(dev, "cport logging finished\n");
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ":\t\tCPORT\n");
	dev_err(dev, ":---------------------------------------------------\n");
#endif
	while (cur_ptr < CPORT_BUF_SIZE) {
		switch (cur_ptr) {
		case CPORT_TX_BUF_PTR:
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
			pr_memlog(mgr->mem_log, ": \t\tTX BUF (%d)\n", ((log_cport->ptr >> 0) & 0x3F)/2);
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
			dev_err(dev, ":---------------------------------------------------\n");
			dev_err(dev, ": \t\tTX BUF (%d)\n", ((log_cport->ptr >> 0) & 0x3F)/2);
			dev_err(dev, ":---------------------------------------------------\n");
#endif
			break;
		case  CPORT_RX_BUF_PTR:
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
			pr_memlog(mgr->mem_log, ": \t\tRX BUF (%d)\n", ((log_cport->ptr >> 8) & 0x3F)/2);
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
			dev_err(dev, ":---------------------------------------------------\n");
			dev_err(dev, ": \t\tRX BUF (%d)\n", ((log_cport->ptr >> 8) & 0x3F)/2);
			dev_err(dev, ":---------------------------------------------------\n");
#endif
			break;
		case CPORT_LOG_PTR_OFFSET:
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
			pr_memlog(mgr->mem_log, ": \t\tCPORT LOG PTR\n");
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
			dev_err(dev, ":---------------------------------------------------\n");
			dev_err(dev, ": \t\tCPORT LOG PTR\n");
			dev_err(dev, ":---------------------------------------------------\n");
#endif
			break;
		case CPORT_UTRL_PTR:
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
			pr_memlog(mgr->mem_log, ": \t\tUTRL\n");
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
			dev_err(dev, ":---------------------------------------------------\n");
			dev_err(dev, ": \t\tUTRL\n");
			dev_err(dev, ":---------------------------------------------------\n");
#endif
			tag = -1;
			idx = 0;
			break;
		case CPORT_UCD_PTR:
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
			pr_memlog(mgr->mem_log, ": \t\tUCD\n");
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
			dev_err(dev, ":---------------------------------------------------\n");
			dev_err(dev, ": \t\tUCD\n");
			dev_err(dev, ":---------------------------------------------------\n");
#endif
			tag = -1;
			idx = 0;
			break;
		case CPORT_UTMRL_PTR:
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
			pr_memlog(mgr->mem_log, ": \t\tUTMRL\n");
			pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
			dev_err(dev, ":---------------------------------------------------\n");
			dev_err(dev, ": \t\tUTMRL\n");
			dev_err(dev, ":---------------------------------------------------\n");
#endif
			break;
		default:
			break;
		}

		if (cur_ptr == CPORT_LOG_PTR_OFFSET) {
			pr_memlog(mgr->mem_log, "%02x%02x%02x%02x ",
				*(buf_ptr_out+0x3), *(buf_ptr_out+0x2), *(buf_ptr_out+0x1), *(buf_ptr_out+0x0));
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
			dev_err(dev, "%02x%02x%02x%02x ",
				*(buf_ptr_out+0x3), *(buf_ptr_out+0x2), *(buf_ptr_out+0x1), *(buf_ptr_out+0x0));
#endif
			buf_ptr_out += 0x100;
			cur_ptr += 0x100;
		} else {
			tag_printed = 0;
			if (cur_ptr >= CPORT_UTRL_PTR &&
					cur_ptr < CPORT_UTMRL_PTR) {
				if (idx++ % 2 == 0) {
					tag_printed = 1;
					tag++;
				}
			}
			__print_cport(dev, tag_printed, tag,
					(u32 *)buf_ptr_out, mgr);
			buf_ptr_out += 0x10;
			cur_ptr += 0x10;
		}
	}
}

static void __ufs_print_cmd_log(struct ufs_dbg_mgr *mgr, struct device *dev) {
	struct ufs_cmd_info *cmd_info = &mgr->cmd_info;
	struct cmd_data *data = cmd_info->data;
	u32 i;
	u32 last;
	u32 max = MAX_CMD_LOGS;
	unsigned long flags;
	u32 total;

	spin_lock_irqsave(&mgr->cmd_lock, flags);
	total = cmd_info->total;
	if (cmd_info->total < max)
		max = cmd_info->total;
	last = (cmd_info->last + MAX_CMD_LOGS - 1) % MAX_CMD_LOGS;
	spin_unlock_irqrestore(&mgr->cmd_lock, flags);

	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
	pr_memlog(mgr->mem_log, ":\t\tSCSI CMD(%u)\n", total - 1);
	pr_memlog(mgr->mem_log, ":---------------------------------------------------\n");
	pr_memlog(mgr->mem_log, ": OP, TAG, LBA, SCT, RETRIES, STIME, ETIME, REQS\n\n");
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ":\t\tSCSI CMD(%u)\n", total - 1);
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ": OP, TAG, LBA, SCT, RETRIES, STIME, ETIME, REQS\n\n");
#endif

	for (i = 0 ; i < max ; i++, data++) {
		pr_memlog(mgr->mem_log, ": 0x%02x, %02d, 0x%08lx, 0x%04x, %d, %llu, %llu, 0x%lx %s\n",
				data->op,
				data->tag,
				data->lba,
				data->sct,
				data->retries,
				data->start_time,
				data->end_time,
				data->outstanding_reqs,
				((last == i) ? "<--" : " "));
#ifdef CONFIG_SCSI_UFS_EXYNOS_DUMP_TO_CONSOLE
		dev_err(dev, ": 0x%02x, %02d, 0x%08lx, 0x%04x, %d, %llu, %llu, 0x%lx %s\n",
			data->op,
			data->tag,
			data->lba,
			data->sct,
			data->retries,
			data->start_time,
			data->end_time,
			data->outstanding_reqs,
			((last == i) ? "<--" : " "));
#endif
		if (last == i)
			pr_memlog(mgr->mem_log, "\n");
	}
}

static void __ufs_put_cmd_log(struct ufs_dbg_mgr *mgr, struct cmd_data *cmd_data)
{
	struct ufs_cmd_info *cmd_info = &mgr->cmd_info;
	unsigned long flags;
	struct cmd_data *pdata;

	spin_lock_irqsave(&mgr->cmd_lock, flags);
	pdata = &cmd_info->data[cmd_info->last];
	++cmd_info->total;
	cmd_info->last = (cmd_info->last + 1) % MAX_CMD_LOGS;
	spin_unlock_irqrestore(&mgr->cmd_lock, flags);

	pdata->op = cmd_data->op;
	pdata->tag = cmd_data->tag;
	pdata->lba = cmd_data->lba;
	pdata->sct = cmd_data->sct;
	pdata->retries = cmd_data->retries;
	pdata->start_time = cmd_data->start_time;
	pdata->end_time = 0;
	pdata->outstanding_reqs = cmd_data->outstanding_reqs;
	cmd_info->pdata[cmd_data->tag] = pdata;
}

static void __ufs_print_evt(struct ufs_dbg_mgr *mgr, u32 id,
			     char *err_name)
{
	int i;
	bool found = false;
	struct ufs_event_hist *e;

	if (id >= UFS_EVT_CNT)
		return;

	e = &mgr->ufs_stats.event[id];

	for (i = 0; i < UFS_EVENT_HIST_LENGTH; i++) {
		int p = (i + e->pos) % UFS_EVENT_HIST_LENGTH;

		if (e->tstamp[p] == 0)
			continue;
		pr_memlog(mgr->mem_log, "%s[%d] = 0x%x at %lld us\n", err_name, p,
		       e->val[p], ktime_to_us(e->tstamp[p]));
		found = true;
	}

	if (!found)
		pr_memlog(mgr->mem_log, "No record of %s\n", err_name);
}

static void __ufs_print_evt_hist(struct ufs_dbg_mgr *mgr)
{
	__ufs_print_evt(mgr, UFS_EVT_PA_ERR, "pa_err");
	__ufs_print_evt(mgr, UFS_EVT_DL_ERR, "dl_err");
	__ufs_print_evt(mgr, UFS_EVT_NL_ERR, "nl_err");
	__ufs_print_evt(mgr, UFS_EVT_TL_ERR, "tl_err");
	__ufs_print_evt(mgr, UFS_EVT_DME_ERR, "dme_err");
	__ufs_print_evt(mgr, UFS_EVT_AUTO_HIBERN8_ERR,
			 "auto_hibern8_err");
	__ufs_print_evt(mgr, UFS_EVT_FATAL_ERR, "fatal_err");
	__ufs_print_evt(mgr, UFS_EVT_LINK_STARTUP_FAIL,
			 "link_startup_fail");
	__ufs_print_evt(mgr, UFS_EVT_RESUME_ERR, "resume_fail");
	__ufs_print_evt(mgr, UFS_EVT_SUSPEND_ERR,
			 "suspend_fail");
	__ufs_print_evt(mgr, UFS_EVT_DEV_RESET, "dev_reset");
	__ufs_print_evt(mgr, UFS_EVT_HOST_RESET, "host_reset");
	__ufs_print_evt(mgr, UFS_EVT_ABORT, "task_abort");
}

/*
 * EXTERNAL FUNCTIONS
 *
 * There are two classes that are to initialize data structures for debug
 * and to define actual behavior.
 */
void exynos_ufs_dump_info(struct ufs_hba *hba,
			  struct ufs_vs_handle *handle, struct device *dev)
{
	struct ufs_dbg_mgr *mgr = (struct ufs_dbg_mgr *)handle->private;

	if (mgr->active == 0)
		goto out;

	mgr->time = cpu_clock(raw_smp_processor_id());

	/* get context */
	__ufs_get_sfr(mgr, ufs_log_sfr);
	__ufs_get_attr(mgr, ufs_log_attr);
	memcpy(&mgr->ufs_stats, &hba->ufs_stats, sizeof(struct ufs_stats));

	/* show context */
	__ufs_print_sfr(handle, dev, ufs_log_sfr);
	__ufs_print_attr(handle, dev, ufs_log_attr);
	__ufs_print_cport(mgr, dev);
	__ufs_print_cmd_log(mgr, dev);
	__ufs_print_evt_hist(mgr);

	if (mgr->first_time == 0ULL)
		mgr->first_time = mgr->time;
out:
	return;
}

void exynos_ufs_cmd_log_start(struct ufs_vs_handle *handle,
				struct ufs_hba *hba, struct scsi_cmnd *cmd)
{
	struct ufs_dbg_mgr *mgr = (struct ufs_dbg_mgr *)handle->private;
	int cpu = raw_smp_processor_id();
	struct cmd_data *cmd_log = &mgr->cmd_log;	/* temp buffer to put */
	unsigned long lba = (cmd->cmnd[2] << 24) |
					(cmd->cmnd[3] << 16) |
					(cmd->cmnd[4] << 8) |
					(cmd->cmnd[5] << 0);
	unsigned int sct = (cmd->cmnd[7] << 8) |
					(cmd->cmnd[8] << 0);

	if (mgr->active == 0)
		return;

	cmd_log->start_time = cpu_clock(cpu);
	cmd_log->op = cmd->cmnd[0];
	cmd_log->tag = cmd->request->tag;
	/* This function runtime is protected by spinlock from outside */
	cmd_log->outstanding_reqs = hba->outstanding_reqs;

	/* unmap */
	if(cmd->cmnd[0] != UNMAP)	// TODO: for unmap
		cmd_log->lba = lba;

	cmd_log->sct = sct;
	cmd_log->retries = cmd->allowed;

	__ufs_put_cmd_log(mgr, cmd_log);
}

void exynos_ufs_cmd_log_end(struct ufs_vs_handle *handle,
				struct ufs_hba *hba, int tag)
{
	struct ufs_dbg_mgr *mgr = (struct ufs_dbg_mgr *)handle->private;
	struct ufs_cmd_info *cmd_info = &mgr->cmd_info;
	int cpu = raw_smp_processor_id();

	if (mgr->active == 0)
		return;

	if (!cmd_info->pdata[tag]) {
		pr_err("%s: there is no cmd logging inform about tag: %d\n",
				__func__, tag);
		return;
	}
	cmd_info->pdata[tag]->end_time = cpu_clock(cpu);
}

int exynos_ufs_dbg_set_lanes(struct ufs_vs_handle *handle,
				struct device *dev, u32 lanes)
{
	struct ufs_dbg_mgr *mgr = (struct ufs_dbg_mgr *)handle->private;
	int ret = 0;

	mgr->lanes = lanes;

	if (mgr->lanes > ATTR_NUM_MAX_LANES) {
		printk("%s: input lanes is too big: %u > %d\n",
				__func__, mgr->lanes, ATTR_NUM_MAX_LANES);
		ret = -1;
	}

	return ret;
}

static int ufs_memlog_file_completed(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int ufs_memlog_status_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int ufs_memlog_level_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static int ufs_memlog_enable_notify(struct memlog_obj *obj, u32 flags)
{
	/* NOP */
	return 0;
}

static const struct memlog_ops ufs_memlog_ops = {
	.file_ops_completed = ufs_memlog_file_completed,
	.log_status_notify = ufs_memlog_status_notify,
	.log_level_notify = ufs_memlog_level_notify,
	.log_enable_notify = ufs_memlog_enable_notify,
};

int exynos_ufs_init_mem_log(struct platform_device *pdev)
{
	struct exynos_ufs_memlog *memlog = &ufs_memlog[ufs_dbg_mgr_idx];
	struct memlog *desc;
	struct memlog_obj *log_obj;
	int ret;

	pr_info("%s: +++\n", __func__);

	ret = memlog_register("UFS", &pdev->dev, &desc);
	if (ret) {
		pr_err("failed to register memlog\n");
		return -1;
	}

	memlog->desc = desc;
	desc->ops = ufs_memlog_ops;

	log_obj = memlog_alloc_printf(desc,
					SZ_512K,
					NULL,
					"log-mem",
					0);

	if (log_obj) {
		memlog->log_obj = log_obj;
		memlog->log_enable = 1;
	} else {
		pr_err("%s: failed to alloc memlog memory for log\n",
			__func__);
		return -1;
	}
	pr_info("%s: ---\n", __func__);

	return 0;
}

int exynos_ufs_init_dbg(struct ufs_vs_handle *handle)
{
	struct ufs_dbg_mgr *mgr;
	int ret = -1;

	if (ufs_dbg_mgr_idx >= DBG_NUM_OF_HOSTS)
		goto out;

	mgr = &ufs_dbg[ufs_dbg_mgr_idx];
	mgr->mem_log = &ufs_memlog[ufs_dbg_mgr_idx];
	ufs_dbg_mgr_idx++;

	handle->private = (void *)mgr;
	mgr->handle = handle;
	mgr->active = 1;

	/* cmd log */
	spin_lock_init(&mgr->cmd_lock);
	ret = 0;
out:
	return ret;
}
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos UFS debug information");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
