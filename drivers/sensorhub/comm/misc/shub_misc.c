/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include "../../utility/shub_utility.h"

extern int register_misc_dev_batch(bool en);
extern int register_misc_dev_injection(bool en);
extern int register_misc_dev_scontext(bool en);

typedef int (*miscdev_init)(bool en);

miscdev_init misc_dev_init_funcs[] =
{
	register_misc_dev_batch,
	register_misc_dev_injection,
	register_misc_dev_scontext,
};

int initialize_shub_misc_dev(void)
{
	int ret;
	uint64_t i, j;

	for (i = 0 ; i < ARRAY_SIZE(misc_dev_init_funcs); i++) {
		ret = misc_dev_init_funcs[i](true);
		if (ret) {
			shub_errf("init[%d] failed. ret %d", (int)i, ret);
			break;
		}
	}

	if (i < ARRAY_SIZE(misc_dev_init_funcs)) {
		j = i;
		for (i = 0; i < j; i++)
			misc_dev_init_funcs[i](false);
	}

	return ret;
}

void remove_shub_misc_dev(void)
{
	uint64_t i;

	for (i = 0 ; i < ARRAY_SIZE(misc_dev_init_funcs); i++)
		misc_dev_init_funcs[i](false);
}
