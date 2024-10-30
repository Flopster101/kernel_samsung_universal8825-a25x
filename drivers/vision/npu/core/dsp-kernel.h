/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_KERNEL_H__
#define __DSP_KERNEL_H__

#include <linux/mutex.h>

#include "dsp-util.h"
#include "dl/dsp-dl-engine.h"

#define DSP_KERNEL_MAX_COUNT		(256)

struct npu_device;

struct dsp_kernel {
	int				id;
	unsigned int			name_length;
	char				*name;
	unsigned int			ref_count;
	void				*elf;
	size_t				elf_size;

	struct list_head		list;
	struct list_head		graph_list;
	struct dsp_kernel_manager	*owner;
};

struct dsp_kernel_manager {
	struct list_head		kernel_list;
	unsigned int			kernel_count;

	struct mutex			lock;
	unsigned int			dl_init;
	struct dsp_dl_param		dl_param;
	struct dsp_util_bitmap		kernel_map;
	struct npu_device		*device;
};

// void dsp_kernel_dump(struct dsp_kernel_manager *kmgr);

int dsp_add_kernel(struct npu_device *device, void *kernel_name);
void dsp_remove_kernel(struct npu_device *device);

int dsp_kernel_manager_open(struct npu_system *system, struct dsp_kernel_manager *kmgr);
void dsp_kernel_manager_close(struct dsp_kernel_manager *kmgr,
		unsigned int count);
int dsp_kernel_manager_probe(struct npu_device *device);
void dsp_kernel_manager_remove(struct dsp_kernel_manager *kmgr);

int dsp_graph_add_kernel(struct npu_device *device, struct npu_session *session, void *kernel_name);
void dsp_graph_remove_kernel(struct npu_device *device, struct npu_session *session);

npu_s_param_ret dsp_kernel_param_handler(struct npu_session *sess, struct vs4l_param *param, int *retval);
#endif
