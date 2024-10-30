/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020-2021 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "inc/tfa_sysfs.h"

static struct class *g_tfa_class;

static int __init tfa98xx_sysfs_init(void)
{
	int ret = 0;

	if (!g_tfa_class)
		g_tfa_class = class_create(THIS_MODULE, TFA_CLASS_NAME);

	pr_info("%s: g_tfa_class=%p\n", __func__, g_tfa_class);
	pr_info("%s: initialized\n", __func__);

	if (!g_tfa_class)
		return -EFAULT;

#if defined(TFA_USE_TFACAL_NODE)
	ret = tfa98xx_cal_init(g_tfa_class);
#endif /* TFA_USE_TFACAL_NODE */

#if defined(TFA_USE_TFALOG_NODE)
	ret = tfa98xx_log_init(g_tfa_class);
#endif /* TFA_USE_TFALOG_NODE */

#if defined(TFA_USE_TFAVVAL_NODE)
	ret = tfa98xx_vval_init(g_tfa_class);
#endif /* TFA_USE_TFAVVAL_NODE */

#if defined(TFA_USE_TFASTC_NODE)
	ret = tfa98xx_stc_init(g_tfa_class);
#endif /* TFA_USE_TFASTC_NODE */

	return ret;
}
module_init(tfa98xx_sysfs_init);

static void __exit tfa98xx_sysfs_exit(void)
{
#if defined(TFA_USE_TFACAL_NODE)
	tfa98xx_cal_exit(g_tfa_class);
#endif /* TFA_USE_TFACAL_NODE */

#if defined(TFA_USE_TFALOG_NODE)
	tfa98xx_log_exit(g_tfa_class);
#endif /* TFA_USE_TFALOG_NODE */

#if defined(TFA_USE_TFAVVAL_NODE)
	tfa98xx_vval_exit(g_tfa_class);
#endif /* TFA_USE_TFAVVAL_NODE */

#if defined(TFA_USE_TFASTC_NODE)
	tfa98xx_stc_exit(g_tfa_class);
#endif /* TFA_USE_TFASTC_NODE */

	class_destroy(g_tfa_class);
	pr_info("exited\n");
}
module_exit(tfa98xx_sysfs_exit);

MODULE_DESCRIPTION("ASoC TFA98XX sysfs node driver");
MODULE_LICENSE("GPL");

