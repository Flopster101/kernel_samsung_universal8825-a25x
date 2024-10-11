// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "dsp-hw-common-system.h"
#include "dsp-hw-common-clk.h"

void dsp_hw_common_clk_dump(struct dsp_clk *clk)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		name = clk->array[count].name;
		freq = clk_get_rate(clk->array[count].clk);
		if (!freq)
			continue;

		dsp_dbg("%32s(%02ld) : %4lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

void dsp_hw_common_clk_user_dump(struct dsp_clk *clk, struct seq_file *file)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		name = clk->array[count].name;
		freq = clk_get_rate(clk->array[count].clk);
		seq_printf(file, "%32s(%ld) : %4lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

int dsp_hw_common_clk_enable(struct dsp_clk *clk)
{
	int ret;
	long count;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		ret = clk_prepare_enable(clk->array[count].clk);
		if (ret) {
			dsp_err("Failed to enable [%s(%ld/%u)]clk(%d)\n",
					clk->array[count].name, count,
					clk->array_size, ret);
			goto p_err;
		}
	}

	clk->ops->dump(clk);
	dsp_leave();
	return 0;
p_err:
	for (count -= 1; count >= 0; --count)
		clk_disable_unprepare(clk->array[count].clk);

	return ret;
}

int dsp_hw_common_clk_disable(struct dsp_clk *clk)
{
	long count;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count)
		clk_disable_unprepare(clk->array[count].clk);

	dsp_leave();
	return 0;
}

int dsp_hw_common_clk_open(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_common_clk_close(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_hw_common_clk_probe(struct dsp_clk *clk, void *sys)
{
	int ret;
	long count;

	dsp_enter();
	clk->sys = sys;
	clk->dev = clk->sys->dev;

	for (count = 0; count < clk->array_size; ++count) {
		clk->array[count].clk = devm_clk_get(clk->dev,
				clk->array[count].name);
		if (IS_ERR(clk->array[count].clk)) {
			ret = IS_ERR(clk->array[count].clk);
			dsp_err("Failed to get [%s(%ld/%u)]clk(%d)\n",
					clk->array[count].name, count,
					clk->array_size, ret);
			goto p_err;
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_hw_common_clk_remove(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
}

int dsp_hw_common_clk_set_ops(struct dsp_clk *clk, const void *ops)
{
	dsp_enter();
	clk->ops = ops;
	dsp_leave();
	return 0;
}
