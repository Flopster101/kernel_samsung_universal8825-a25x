// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "panel.h"
#include "panel_obj.h"
#include "panel_debug.h"

/**
 * create_dumpinfo - create a struct dumpinfo structure
 * @name: pointer to a string for the name of this dump.
 * @res: pointer to a resource.
 * @ops: function to be called when execute dump.
 * @expects: pointer to a expectation array to check panel register state.
 * @nr_expects: size of expectation array.
 *
 * This is used to create a struct dumpinfo pointer.
 *
 * Returns &struct dumpinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_dumpinfo().
 */
struct dumpinfo *create_dumpinfo(char *name,
		struct resinfo *res, struct dump_ops *ops,
		struct dump_expect *expects, unsigned int nr_expects)
{
	struct dumpinfo *dump;
	int i;

	if (!name || !res || !ops) {
		panel_err("invalid parameter\n");
		return NULL;
	}

	dump = kzalloc(sizeof(*dump), GFP_KERNEL);
	if (!dump)
		return NULL;

	dump->res = res;
	memcpy(&dump->ops, ops, sizeof(*ops));
	if (expects) {
		dump->expects = kmemdup(expects,
				sizeof(*expects) * nr_expects, GFP_KERNEL);
		dump->nr_expects = nr_expects;
		for (i = 0; i < nr_expects; i++)
			dump->expects[i].msg = expects[i].msg ?
				kstrndup(expects[i].msg, SZ_128, GFP_KERNEL) : NULL;
	}
	pnobj_init(&dump->base, CMD_TYPE_DMP, name);

	return dump;
}
EXPORT_SYMBOL(create_dumpinfo);

/**
 * destroy_dumpinfo - destroys a struct dumpinfo structure
 * @rx_packet: pointer to the struct dumpinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_dumpinfo().
 */
void destroy_dumpinfo(struct dumpinfo *dump)
{
	int i;

	if (!dump)
		return;
	
	pnobj_deinit(&dump->base);
	for (i = 0; i < dump->nr_expects; i++)
		kfree(dump->expects[i].msg);
	kfree(dump->abd_print);
	kfree(dump->expects);
	kfree(dump);
}
EXPORT_SYMBOL(destroy_dumpinfo);
