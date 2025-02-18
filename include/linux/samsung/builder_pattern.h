// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020-2021 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#ifndef __BUILDER_PATTERN_H__
#define __BUILDER_PATTERN_H__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/of.h>

#include <linux/samsung/jump_table_target.h>

/* An interface 'Builder' struct
 * TODO: This struct should be embedded if the drvdata use
 * this 'Builder Pattern'.
 */
struct builder {
	struct device *dev;
};

#define DT_BUILDER(__parse_dt) \
	{ .parse_dt = __parse_dt, }

/* The prototype of 'Concrete Builde' parsing a device tree */
typedef int (*parse_dt_t)(struct builder *bd, struct device_node *np);

struct dt_builder {
	parse_dt_t parse_dt;
};

#define DEVICE_BUILDER(__construct_dev, __destruct_dev) \
	{ .construct_dev = __construct_dev, .destruct_dev = __destruct_dev, }

/* The prototype of 'Concrete Builde' constructing a device */
typedef int (*construct_dev_t)(struct builder *bd);

/* The prototype of 'Concrete Builde' destructing a device */
typedef void (*destruct_dev_t)(struct builder *bd);

struct dev_builder {
	construct_dev_t construct_dev;
	destruct_dev_t destruct_dev;
};

/* A common 'Director' parsing a device tree
 * @return - 0 on success. 'errno' of last failed on failure.
 */
static __always_inline int sec_director_parse_dt(struct builder *bd,
		const struct dt_builder *builder, size_t n)
{
	struct device *dev = bd->dev;
	struct device_node *np = dev_of_node(dev);
	int err = 0;
	size_t i;

	for (i = 0; i < n; i++) {
		parse_dt_t parse_dt = builder[i].parse_dt;

		err = parse_dt(bd, np);
		if (err) {
			dev_err(dev, "failed to parse a device tree - %ps (%d)\n",
					jump_table_target(parse_dt), err);
			return err;
		}
	}

	return err;
}

/* A common 'Director' constructing a device
 * @last_failed - The number of called builders on success.
 *           The "NEGATIVE" index of last failed on failure.
 * @return - 0 on success.
 *           return value of the last concrete builder on failure.
 */
static __always_inline int sec_director_construct_dev(struct builder *bd,
		const struct dev_builder *builder, ssize_t n,
		ssize_t *last_failed)
{
	struct device *dev = bd->dev;
	int err;
	ssize_t i;

	for (i = 0; i < n; i++) {
		construct_dev_t construct_dev = builder[i].construct_dev;

		if (!construct_dev)
			continue;

		err = construct_dev(bd);
		if (err) {
			dev_err(dev, "failed to construct_dev a device - %ps (%d)\n",
					jump_table_target(construct_dev), err);
			*last_failed = -i;
			return err;
		}
	}

	*last_failed = n;

	return 0;
}

/* A common 'Director' destructing a device */
static __always_inline void sec_director_destruct_dev(struct builder *bd,
		const struct dev_builder *builder, ssize_t n,
		ssize_t last_failed)
{
	ssize_t i;

	BUG_ON((last_failed > n) || (last_failed < 0));

	for (i = last_failed - 1; i >= 0; i--) {
		destruct_dev_t destruct_dev = builder[i].destruct_dev;

		if (!destruct_dev)
			continue;

		destruct_dev(bd);
	}
}

/* A wrapper function for probe call-backs */
static __always_inline int sec_director_probe_dev(struct builder *bd,
		const struct dev_builder *builder, ssize_t n)
{
	int err;
	ssize_t last_failed;

	err = sec_director_construct_dev(bd, builder, n, &last_failed);
	if (last_failed <= 0)
		goto err_dev_director;

	return 0;

err_dev_director:
	sec_director_destruct_dev(bd, builder, n, -last_failed);
	return err;
}

#endif /* __BUILDER_PATTERN_H__ */
