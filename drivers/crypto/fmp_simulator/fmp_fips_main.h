/*
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _FMP_FIPS_MAIN_H_
#define _FMP_FIPS_MAIN_H_

int exynos_fmp_fips_init(struct exynos_fmp *fmp);
int exynos_fmp_fips_register(struct exynos_fmp *fmp);
void exynos_fmp_fips_deregister(struct exynos_fmp *fmp);

#endif /* _FMP_FIPS_MAIN_H_ */
