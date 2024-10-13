/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/sched/clock.h>
#include <asm-generic/sections.h>

#include <soc/samsung/debug-snapshot.h>
#include "debug-snapshot-local.h"

#define DSS_QD_MAX_NUM_ENTRIES		(48)
#define DSS_QD_MAX_NAME_LENGTH		(16)
#define DSS_QD_ATTR_LOG			(0)
#define DSS_QD_ATTR_ARRAY		(1)
#define DSS_QD_ATTR_STRUCT		(2)
#define DSS_QD_ATTR_STACK		(3)
#define DSS_QD_ATTR_SYMBOL		(4)
#define DSS_QD_ATTR_BINARY		(5)
#define DSS_QD_ATTR_ELF			(6)
#define DSS_QD_ATTR_SKIP_ENCRYPT	(1 << 31)

struct dbg_snapshot_qd_region {
	char	name[DSS_QD_MAX_NAME_LENGTH];
	char	struct_name[DSS_QD_MAX_NAME_LENGTH];
	u64	virt_addr;
	u64	phys_addr;
	u64	size;
	u32	attr;
	u32	magic;
};

struct dbg_snapshot_qd_table {
	struct dbg_snapshot_qd_region	*entry;
	u32				num_regions;
	u32				magic_key;
	u32				version;
};

static struct dbg_snapshot_qd_table dss_qd_table;
static DEFINE_SPINLOCK(qd_lock);
static struct device *qdump_dev = NULL;

int dbg_snapshot_qd_add_region(void *v_entry, u32 attr)
{
	struct dbg_snapshot_qd_region *entry =
			(struct dbg_snapshot_qd_region *)v_entry;
	struct dbg_snapshot_qd_region *qd_entry;
	u32 entries, i;
	int ret = 0;

	if (!entry || !entry->virt_addr ||
			!entry->phys_addr || !strlen(entry->name)) {
		dev_err(qdump_dev, "Invalid entry details\n");
		return -EINVAL;
	}

	if ((strlen(entry->name) > DSS_QD_MAX_NAME_LENGTH) ||
		(strlen(entry->struct_name) > DSS_QD_MAX_NAME_LENGTH)) {
		dev_err(qdump_dev, "over string names\n");
		return -EINVAL;
	}

	spin_lock(&qd_lock);

	entries = dss_qd_table.num_regions;
	if (entries >= DSS_QD_MAX_NUM_ENTRIES) {
		dev_err(qdump_dev, "entries is full, No allowed more\n");
		spin_unlock(&qd_lock);
		return -ENOMEM;
	}

	for (i = 0; i < entries; i++) {
		qd_entry = &dss_qd_table.entry[i];
		if (!strncmp(qd_entry->name, entry->name,
					strlen(entry->name))) {
			dev_err(qdump_dev, "entry name is exist in array : %s",
				entry->name);
			spin_unlock(&qd_lock);
			return -EINVAL;
		}
	}

	qd_entry = &dss_qd_table.entry[entries];
	strlcpy(qd_entry->name, entry->name, sizeof(qd_entry->name));
	strlcpy(qd_entry->struct_name, entry->struct_name, sizeof(qd_entry->struct_name));
	qd_entry->virt_addr = entry->virt_addr;
	qd_entry->phys_addr = entry->phys_addr;
	qd_entry->size = entry->size;
	qd_entry->magic = DSS_SIGN_MAGIC | entries;
	qd_entry->attr = attr;

	dss_qd_table.num_regions = entries + 1;

	spin_unlock(&qd_lock);

	dev_info(qdump_dev, "Success to add (0x%x: %s)\n",
			entries, qd_entry->name);

	return ret;
}
EXPORT_SYMBOL(dbg_snapshot_qd_add_region);

bool dbg_snapshot_qd_enable(void)
{
	bool ret = false;

	if (dss_dpm.dump_mode == QUICK_DUMP)
		ret = true;

	return ret;
}
EXPORT_SYMBOL(dbg_snapshot_qd_enable);

static void register_dss_log_item(void)
{
	struct dbg_snapshot_qd_region qd_entry;
	int item_num = dbg_snapshot_log_get_num_items();
	struct dbg_snapshot_log_item *item;
	int i;

	for (i = 0; i < item_num; i++){
		item = (struct dbg_snapshot_log_item *)
			dbg_snapshot_log_get_item_by_index(i);
		if (item->entry.enabled) {
			strlcpy(qd_entry.name, item->name,
					sizeof(qd_entry.name));
			strlcpy(qd_entry.struct_name, item->name,
					sizeof(qd_entry.struct_name));
			qd_entry.virt_addr = item->entry.vaddr;
			qd_entry.phys_addr = item->entry.paddr;
			qd_entry.size = item->entry.size;
			if (dbg_snapshot_qd_add_region(&qd_entry,
						DSS_QD_ATTR_STRUCT))
				dev_err(qdump_dev, "Failed to add "
						"dss_log_item : %s\n",
						item->name);
		}
	}
}

static void register_dss_item(void)
{
	struct dbg_snapshot_qd_region qd_entry;
	int item_num = dbg_snapshot_get_num_items();
	struct dbg_snapshot_item *item;
	int i;

	for (i = 0; i < item_num; i++){
		if (i == DSS_ITEM_KEVENTS_ID)
			continue;
		item = (struct dbg_snapshot_item *)
			dbg_snapshot_get_item_by_index(i);
		if (item->entry.enabled) {
			strlcpy(qd_entry.name, item->name,
					sizeof(qd_entry.name));
			qd_entry.struct_name[0] = '\0';
			qd_entry.virt_addr = item->entry.vaddr;
			qd_entry.phys_addr = item->entry.paddr;
			qd_entry.size = item->entry.size;
			if (dbg_snapshot_qd_add_region(&qd_entry,
						DSS_QD_ATTR_BINARY))
				dev_err(qdump_dev, "Failed to add "
						"dss_item : %s\n",
						item->name);
		}
	}
}

static void dbg_snapshot_qd_list_dump(void)
{
	struct dbg_snapshot_qd_region *entry;
	unsigned long i, size = 0;

	if (!qdump_dev)
		return;

	dev_info(qdump_dev, "quickdump physical / virtual memory layout:\n");
	for (i = 0; i < dss_qd_table.num_regions; i++) {
		entry = &dss_qd_table.entry[i];
		dev_info(qdump_dev, "%-16s: -%16s: phys:0x%zx / virt:0x%zx / size:0x%zx\n",
			entry->name,
			entry->struct_name,
			entry->phys_addr,
			entry->virt_addr,
			entry->size);

		size += entry->size;
	}

	dev_info(qdump_dev, "total_quick_dump_size: %ldKB, quick_dump_entry: 0x%x\n",
			size / SZ_1K, __raw_readq(dbg_snapshot_get_header_vaddr() + DSS_OFFSET_QD_ENTRY));
}

static int dbg_snapshot_qd_probe(struct platform_device *pdev)
{
	/* Need to wait for checking whether quick-dump is enabled or not */
	if (!dbg_snapshot_get_enable())
		return -EPROBE_DEFER;

	if (dss_dpm.dump_mode != QUICK_DUMP) {
		dev_err(&pdev->dev, "No Quick dump mode\n");
		return -ENODEV;
	}

	qdump_dev = &pdev->dev;
	dss_qd_table.entry = devm_kzalloc(qdump_dev, (DSS_QD_MAX_NUM_ENTRIES *
				sizeof(struct dbg_snapshot_qd_region)), GFP_KERNEL);
	if (!dss_qd_table.entry) {
		dev_err(&pdev->dev, "failed alloc of dss_qd_table\n");
		dbg_snapshot_set_qd_entry(0);
		return -ENOMEM;
	}

	qdump_dev = &pdev->dev;
	dbg_snapshot_set_qd_entry((unsigned long)dss_qd_table.entry);
	dss_qd_table.num_regions = 0;

	dev_info(&pdev->dev, "Success to initialization\n");

	register_dss_item();
	register_dss_log_item();

	dbg_snapshot_qd_list_dump();

	dev_info(&pdev->dev, "%s successful.\n", __func__);
	return 0;
}

static const struct of_device_id dbg_snapshot_qd_matches[] = {
	{ .compatible = "debug-snapshot,qdump", },
	{},
};
MODULE_DEVICE_TABLE(of, dbg_snapshot_qd_matches);

static struct platform_driver dbg_snapshot_qd_driver = {
	.probe		= dbg_snapshot_qd_probe,
	.driver		= {
		.name	= "debug-snapshot-qdump",
		.of_match_table	= of_match_ptr(dbg_snapshot_qd_matches),
	},
};
module_platform_driver(dbg_snapshot_qd_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Debug-Snapshot-qdump");
