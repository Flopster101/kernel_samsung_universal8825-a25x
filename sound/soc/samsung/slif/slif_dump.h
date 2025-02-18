/* sound/soc/samsung/vts/slif_dump.h
 *
 * ALSA SoC - Samsung vts dump driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_SLIF_LIF_DUMP_H
#define __SND_SOC_SLIF_LIF_DUMP_H

#include <linux/device.h>

/**
 * Report dump data written
 * @param[in]	id		unique buffer id
 * @param[in]	pointer		byte index of the written data
 */
extern void slif_dump_period_elapsed(int id, size_t pointer);

/**
 * Register dump
 * @param[in]	dev		pointer to abox device
 * @param[in]	id		unique buffer id
 * @param[in]	name		unique buffer name
 * @param[in]	area		virtual address of the buffer
 * @param[in]	addr		pysical address of the buffer
 * @param[in]	bytes		buffer size in bytes
 * @return	error code if any
 */
extern int slif_dump_register(struct device *dev, int id, const char *name,
		void *area, phys_addr_t addr, size_t bytes);

/**
 * Initialize slif_lif dump module
 * @param[in]	dev		pointer to device
 */
extern void slif_dump_init(struct device *dev_slif_lif);

#endif /* __SND_SOC_VTS_DUMP_H */
