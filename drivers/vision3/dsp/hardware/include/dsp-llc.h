/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __HW_DSP_LLC_H__
#define __HW_DSP_LLC_H__

#include <linux/mutex.h>

#define DSP_LLC_REGION_NAME_LEN		(32)
#define DSP_LLC_SCENARIO_NAME_LEN	(32)

struct dsp_system;
struct dsp_llc;

struct dsp_llc_region {
	char			name[DSP_LLC_REGION_NAME_LEN];
	unsigned int		idx;
	unsigned int		way;
};

struct dsp_llc_scenario {
	unsigned int		id;
	char			name[DSP_LLC_SCENARIO_NAME_LEN];
	unsigned int		region_count;
	struct dsp_llc_region	*region_list;
	struct mutex		lock;
	bool			enabled;
};

struct dsp_llc_ops {
	int (*get_by_name)(struct dsp_llc *llc, unsigned char *name);
	int (*put_by_name)(struct dsp_llc *llc, unsigned char *name);
	int (*get_by_id)(struct dsp_llc *llc, unsigned int id);
	int (*put_by_id)(struct dsp_llc *llc, unsigned int id);
	int (*all_put)(struct dsp_llc *llc);

	int (*open)(struct dsp_llc *llc);
	int (*close)(struct dsp_llc *llc);
	int (*probe)(struct dsp_llc *llc, void *sys);
	void (*remove)(struct dsp_llc *llc);
};

struct dsp_llc {
	unsigned int			scen_count;
	struct dsp_llc_scenario		*scen;

	const struct dsp_llc_ops	*ops;
	struct dsp_system		*sys;
};

#endif
