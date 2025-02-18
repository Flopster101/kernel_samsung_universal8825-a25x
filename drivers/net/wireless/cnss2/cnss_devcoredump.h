/*
 * This file is provided under the GPLv2 license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2015 Intel Deutschland GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 */
#ifndef __CNSS_DEVCOREDUMP_H
#define __CNSS_DEVCOREDUMP_H

#include <linux/devcoredump.h>
#include "main.h"

#ifdef CONFIG_DEV_COREDUMP
#define cnss_dev_coredumpv dev_coredumpv
#define cnss_dev_coredumpm dev_coredumpm
#define cnss_dev_coredumpsg dev_coredumpsg

static inline int cnss_devcoredump_init(void)
{
	return 0;
}

static inline void cnss_devcoredump_exit(void) {}
#else
void cnss_dev_coredumpv(struct device *dev, void *data, size_t datalen,
		   gfp_t gfp);

void cnss_dev_coredumpm(struct device *dev, struct module *owner,
		   void *data, size_t datalen, gfp_t gfp,
		   ssize_t (*read)(char *buffer, loff_t offset, size_t count,
				   void *data, size_t datalen),
		   void (*free)(void *data));

void cnss_dev_coredumpsg(struct device *dev, struct scatterlist *table,
		    size_t datalen, gfp_t gfp);

int cnss_devcoredump_init(void);
void cnss_devcoredump_exit(void);
#endif /* CONFIG_DEV_COREDUMP */

struct cnss_tlv_dump_data {
	__le32 type;
	__le32 tlv_len;
	u8 tlv_data[];
} __packed;

struct cnss_dump_file_data {
	char df_magic[16];
	__le32 len;
	__le32 version;
	u8 data[0];
} __packed;

struct cnss_dump_info {
	u32 nentries;
	u32 type;
	u32 total_size;
	struct cnss_dump_seg dump_segs[DUMP_MAX_SEG];
};

#define CNSS_FW_CRASH_DUMP_VERSION 1
#endif /* __CNSS_DEVCOREDUMP_H */
