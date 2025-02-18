/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_BINARY_H__
#define __DSP_BINARY_H__

#include <linux/device.h>
#include <linux/firmware.h>

#define DSP_BINARY_NAME_SIZE	(64)

/*
int dsp_binary_load(struct device *dev, const char *name, char *postfix, const char *extension,
		void *target, size_t size, size_t *loaded_size);
int dsp_binary_master_load(struct device *dev, const char *name, char *postfix,
		const char *extension, void __iomem *target, size_t size,
		size_t *loaded_size);
*/
int dsp_binary_alloc_load(struct device *dev, const char *name, char *postfix,
		const char *extension, void **target, size_t *loaded_size);
/*
int dsp_binary_load_async(struct device *dev, const char *name, char *postfix,
		const char *extension, void *context,
		void (*cont)(const struct firmware *fw, void *context));
*/
#endif
