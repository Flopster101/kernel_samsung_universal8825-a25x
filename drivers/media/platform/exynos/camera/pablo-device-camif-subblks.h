// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_DEVICE_CAMIF_SUBBLKS_H
#define PABLO_DEVICE_CAMIF_SUBBLKS_H

struct pablo_camif_subblks_data {
	spinlock_t slock;
	int (*probe)(struct platform_device *pdev,
			struct pablo_camif_subblks_data *data);
	int (*remove)(struct platform_device *pdev,
			struct pablo_camif_subblks_data *data);
};

struct pablo_camif_bns {
	struct device *dev;
	struct pablo_camif_subblks_data *data;

	void __iomem 	*regs;
	void __iomem 	*mux_regs;
	u32 		*mux_val;
	u32 		mux_elems;
	u32		dma_mux_val;

	bool		en;
	u32		width;
	u32		height;
};

#if IS_ENABLED(CONFIG_PABLO_CAMIF_BNS)
struct pablo_camif_bns *pablo_camif_bns_get(void);
void pablo_camif_bns_cfg(struct pablo_camif_bns *bns,
		struct is_sensor_cfg *sensor_cfg,
		u32 ch);
void pablo_camif_bns_reset(struct pablo_camif_bns *bns);
#else
#define pablo_camif_bns_get() 	({ NULL; })
#define pablo_camif_bns_cfg(bns, cfg, ch) do {} while(0)
#define pablo_camif_bns_reset(bns) {} do {} while(0)
#endif

struct pablo_camif_mcb {
	void __iomem		*regs;
	struct mutex		lock;
	unsigned long		active_ch;
};

struct pablo_camif_mcb *pablo_camif_mcb_get(void);

struct pablo_camif_ebuf {
	void __iomem		*regs;
	int			irq;
	struct mutex		lock;
	unsigned int		num_of_ebuf;
};

struct pablo_camif_ebuf *pablo_camif_ebuf_get(void);

#endif /* PABLO_DEVICE_CAMIF_SUBBLKS_H */
