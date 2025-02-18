/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC - Samsung Abox OEM module
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_OEM_H
#define __SND_SOC_ABOX_OEM_H

#include <linux/module.h>

enum ABOX_OEM_RESERVED_MEMORY {
	ABOX_OEM_RESERVED_MEMORY_DBG, /* abox_dbg */
	ABOX_OEM_RESERVED_MEMORY_SLOG,  /* abox_slog */
};

/**
 * Get OEM changed size of each reserved memory
 * @param[in]	type	type of reserved memory
 * @return	size of the reserved memory which is changed by OEM
 */
extern ssize_t abox_oem_resize_reserved_memory(enum ABOX_OEM_RESERVED_MEMORY type);

#endif /* __SND_SOC_ABOX_OEM_H */
