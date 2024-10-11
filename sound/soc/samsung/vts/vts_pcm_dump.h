/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/vts_pcm_dump.h
 *
 * ALSA SoC Audio Layer - Samsung VTS Internal Buffer Dumping driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_PCM_DUMP_H
#define __SND_SOC_VTS_PCM_DUMP_H

#include <linux/device.h>
#include <sound/samsung/vts.h>

/**
 * Report dump data written
 * @param[in]	id		unique buffer id
 * @param[in]	pointer		byte index of the written data
 */
extern void vts_pcm_dump_period_elapsed(int id, size_t pointer);

/**
 * Register vts file only
 * @param[in]	name		unique buffer name
 * @param[in]	data		private data
 * @param[in]	fops		file operation callbacks
 * @return	file entry pointer
 */
extern struct proc_dir_entry *vts_pcm_dump_register_file(const char *name, void *data,
		const struct proc_ops *fops);
/**
 * Destroy registered dump file
 * @param[in]	file		pointer to file entry
 */
extern void vts_pcm_dump_unregister_file(struct proc_dir_entry *file);

/**
 * Get given data of the dump file
 * @param[in]	file		pointer to file entry
 * @return	data which is given at registration
 */
extern void *vts_dump_get_data(const struct file *file);

/**
 * Register vts dump
 * @param[in]	dev		pointer to vts device
 * @param[in]	id		unique buffer id
 * @param[in]	name		unique buffer name
 * @param[in]	area		virtual address of the buffer
 * @param[in]	addr		pysical address of the buffer
 * @param[in]	bytes		buffer size in bytes
 * @return	error code if any
 */
extern int vts_pcm_dump_register(struct device *dev, int id,
		const char *name, void *area, phys_addr_t addr, size_t bytes);

/**
 * Initialize vts dump module
 * @param[in]	dev		pointer to vts device
 */
extern void vts_pcm_dump_init(struct device *dev_vts);
extern bool vts_pcm_dump_get_file_started(int id);
extern void vts_pcm_dump_reset(int id);
#endif /* __SND_SOC_VTS_PCM_DUMP_H */
