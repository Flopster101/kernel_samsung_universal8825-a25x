/*
 * exyswd-rng.c - Random Number Generator driver for the exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 * Jiye Min <jiye.min@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/hw_random.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <soc/samsung/exynos-smc.h>

#define HWRNG_RET_OK			0
#define HWRNG_RET_INVALID_ERROR		1
#define HWRNG_RET_RETRY_ERROR		2
#define HWRNG_RET_TEST_ERROR		3
#define HWRNG_RET_SYNC_SSS_BUSY		9

#define EXYRNG_RETRY_MAX_COUNT		1000000
#define EXYRNG_RANDOM_UNIT_SIZE		64
#define EXYRNG_MAX_RANDOM_BUF_SIZE	(EXYRNG_RANDOM_UNIT_SIZE * 2)

#ifdef CONFIG_EXYRNG_DEBUG
#define exyrng_debug(args...)	pr_debug(args)
#else
#define exyrng_debug(args...)
#endif

struct random_buf {
	u64 *random_buf;
	dma_addr_t buf_pa_addr;
};

struct exyswd_rng_dev {
	struct hwrng rng;
	struct device *dev;
	struct random_buf rbuf;
	spinlock_t hwrandom_lock;
	bool start_up_test;
	bool hwrng_read_flag;
};

void exynos_swd_test_fail(void)
{
	panic("[ExyRNG] It failed to health tests. It means that it detects "
	      "the malfunction of TRNG(HW) which generates random numbers. If it "
	      "doesn't offer enough entropy, it should not be used. The system "
	      "reset could be a way to solve it. The health tests are designed "
	      "to have the false positive rate of approximately once per billion "
	      "based on min-entropy of TRNG.\n");
}

static inline struct exyswd_rng_dev *to_rng_dev(struct hwrng *rng)
{
	return container_of(rng, struct exyswd_rng_dev, rng);
}

static int exynos_cm_smc(u64 *arg0, u64 *arg1,
			 u64 *arg2, u64 *arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(*arg0, *arg1, *arg2, *arg3, 0, 0, 0, 0, &res);

	*arg0 = res.a0;
	*arg1 = res.a1;
	*arg2 = res.a2;
	*arg3 = res.a3;

	return *arg0;
}

static int exynos_swd_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	u64 reg0;
	u64 reg1;
	u64 reg2;
	u64 reg3;
	u32 *read_buf = data;
	u32 read_size = max;
	u32 temp_size;
	u32 retry_cnt;
	u32 i = 0;
	unsigned long flag;
	int ret = HWRNG_RET_OK;
	struct exyswd_rng_dev *dev = to_rng_dev(rng);
	struct random_buf *rbuf = &dev->rbuf;

	retry_cnt = 0;
	temp_size = EXYRNG_RANDOM_UNIT_SIZE;
	while (read_size) {
		spin_lock_irqsave(&dev->hwrandom_lock, flag);

		if (read_size < EXYRNG_RANDOM_UNIT_SIZE)
			temp_size = read_size;

		reg0 = SMC_CMD_RANDOM;
		reg1 = HWRNG_GET_DATA;
		reg2 = rbuf->buf_pa_addr;
		reg3 = temp_size;

		ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);

		spin_unlock_irqrestore(&dev->hwrandom_lock, flag);

		if (ret == HWRNG_RET_RETRY_ERROR || ret == HWRNG_RET_SYNC_SSS_BUSY) {
			if (retry_cnt++ > EXYRNG_RETRY_MAX_COUNT) {
				ret = -EFAULT;
				pr_info("[ExyRNG] exceed retry in read\n");
				goto out;
			}
			continue;
		}

		if (ret == HWRNG_RET_TEST_ERROR) {
			exynos_swd_test_fail();
			ret = -EFAULT;
			goto out;
		}

		if (ret != HWRNG_RET_OK) {
			ret = -EFAULT;
			goto out;
		}

		memcpy((read_buf + (i * temp_size)),
			rbuf->random_buf,
			temp_size);

		read_size -= temp_size;
		retry_cnt = 0;
		i++;
	}

	ret = max;

out:
	return ret;
}

static int exyswd_rng_probe(struct platform_device *pdev)
{
	struct exyswd_rng_dev *rng_dev;
	struct random_buf *rbuf;
	int ret;

	rng_dev = devm_kzalloc(&pdev->dev, sizeof(*rng_dev), GFP_KERNEL);
	if (!rng_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, rng_dev);

	rng_dev->rng.name = "exyswd_rng";
	rng_dev->rng.read = exynos_swd_read;
	rng_dev->rng.quality = 500;
	rng_dev->dev = &pdev->dev;
	rng_dev->start_up_test = 1;

	rbuf = &rng_dev->rbuf;
	rbuf->random_buf = dmam_alloc_coherent(rng_dev->dev,
					       EXYRNG_MAX_RANDOM_BUF_SIZE,
					       &rbuf->buf_pa_addr,
					       __GFP_ZERO);
	if (!rbuf->random_buf) {
		exyrng_debug("[ExyRNG] Fail to allocate memory(random_buf)\n");
		ret = -ENOMEM;
		return ret;
	}

	spin_lock_init(&rng_dev->hwrandom_lock);

	ret = devm_hwrng_register(&pdev->dev, &rng_dev->rng);
	if (ret)
		pr_info("ExyRNG: hwrng registration failed\n");
	else
		pr_info("ExyRNG: hwrng registered\n");

	return ret;
}

static int exyswd_rng_remove(struct platform_device *pdev)
{
	struct exyswd_rng_dev *rng_dev = platform_get_drvdata(pdev);
	struct random_buf *rbuf = &rng_dev->rbuf;

	if (rbuf->random_buf) {
		dma_free_coherent(rng_dev->dev,
				  EXYRNG_MAX_RANDOM_BUF_SIZE,
				  rbuf->random_buf,
				  rbuf->buf_pa_addr);

		rbuf->random_buf = NULL;
		rbuf->buf_pa_addr = 0;
	}

	return 0;
}

static struct platform_driver exyswd_rng_driver = {
	.probe		= exyswd_rng_probe,
	.remove		= exyswd_rng_remove,
	.driver		= {
		.name	= "exyswd_rng",
		.owner	= THIS_MODULE,
	},
};

static struct platform_device exyswd_rng_device = {
	.name = "exyswd_rng",
	.id = -1,
};

static int __init exyswd_rng_init(void)
{
	int ret;

	ret = platform_device_register(&exyswd_rng_device);
	if (ret)
		return ret;

	ret = platform_driver_register(&exyswd_rng_driver);
	if (ret) {
		platform_device_unregister(&exyswd_rng_device);
		return ret;
	}

	pr_info("ExyRNG driver, (c) 2014 Samsung Electronics\n");

	return 0;
}

static void __exit exyswd_rng_exit(void)
{
	platform_driver_unregister(&exyswd_rng_driver);
	platform_device_unregister(&exyswd_rng_device);
}

module_init(exyswd_rng_init);
module_exit(exyswd_rng_exit);

MODULE_DESCRIPTION("EXYNOS H/W Random Number Generator driver");
MODULE_AUTHOR("Jiye Min <jiye.min@samsung.com>");
MODULE_LICENSE("GPL");
