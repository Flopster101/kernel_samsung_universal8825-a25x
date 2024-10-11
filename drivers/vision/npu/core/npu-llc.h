/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_LLC_H_
#define _NPU_LLC_H_

u32 npu_kpi_llc_size(struct npu_scheduler_info *info);
void npu_set_llc(struct npu_scheduler_info *info);
void npu_llc_close(struct npu_scheduler_info *info);

#endif /* _NPU_LLC_H_ */
