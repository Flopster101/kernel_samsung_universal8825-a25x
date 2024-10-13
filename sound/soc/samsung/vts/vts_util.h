/* sound/soc/samsung/vts/vts_util.h
 *
 * ALSA SoC - Samsung VTS utility
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_UTIL_H
#define __SND_SOC_VTS_UTIL_H

#include <sound/pcm.h>
#include <linux/firmware.h>

/**
 * Request memory resource and map to virtual address
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of resource
 * @param[out]	phys_addr	physical address of the resource
 * @param[out]	size		size of the resource
 * @return	virtual address
 */
extern void __iomem *vts_devm_get_request_ioremap(struct platform_device *pdev,
		const char *name, phys_addr_t *phys_addr, size_t *size);

#endif /* __SND_SOC_ABOX_UTIL_H */
