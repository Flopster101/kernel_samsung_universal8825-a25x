/*
 * snd-dbmdx.c -- ASoC Machine Driver for DBMDX
 *
 * Copyright (C) 2014 DSP Group
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* #define DEBUG */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#endif

#include "dbmdx-interface.h"

#define DRIVER_NAME "snd-dbmdx-mach-drv"
#define CODEC_NAME "dbmdx" /* "dbmdx" */
#define PLATFORM_DEV_NAME "dbmdx-snd-soc-platform"

static int board_dai_init(struct snd_soc_pcm_runtime *rtd);

struct snd_soc_dai_link_component cpus[] = {
	{
		.dai_name = "DBMDX_codec_dai",
	},
};

struct snd_soc_dai_link_component codecs[] = {
	{
		.dai_name = "DBMDX_codec_dai",
	},
};

struct snd_soc_dai_link_component platforms[] = {
	{
		.dai_name = "DBMDX_platform_dai",
	},
};

static struct snd_soc_dai_link board_dbmdx_dai_link[] = {
	{
		.name = "dbmdx_dai_link.1",
		.stream_name = "voice_sv",
		/* asoc Cpu-Dai  device name */
		.cpus = cpus,
		.num_cpus = ARRAY_SIZE(cpus),
		/* asoc Codec-Dai device name */
		.codecs = codecs,
		.num_codecs = ARRAY_SIZE(codecs),
		/* asoc Platform-Dai device name */
		.platforms = platforms,
		.num_platforms = ARRAY_SIZE(platforms),
		.init = board_dai_init,
	},
};

static int board_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct snd_soc_card dspg_dbmdx_card = {
	.name		= "dspg-dbmdx",
	.driver_name    = "dspgdbmdxcodec",
	.dai_link	= board_dbmdx_dai_link,
	.num_links	= ARRAY_SIZE(board_dbmdx_dai_link),
	.set_bias_level		= NULL,
	.set_bias_level_post	= NULL,
	.owner        = THIS_MODULE,
};

#if IS_ENABLED(CONFIG_OF)
static int dbmdx_init_dai_link(struct snd_soc_card *card)
{
	int cnt;
	struct snd_soc_dai_link *dai_link;
	struct device_node *codec_node, *platform_node;

	codec_node = of_find_node_by_name(0, CODEC_NAME);
	if (!of_device_is_available(codec_node))
		codec_node = of_find_node_by_name(codec_node, CODEC_NAME);

	if (!codec_node) {
		pr_err("%s: Codec node not found\n", __func__);
		return -EINVAL;
	}

	platform_node = of_find_node_by_name(0, PLATFORM_DEV_NAME);
	if (!platform_node) {
		pr_err("%s: Platform node not found\n", __func__);
		return -EINVAL;
	}

	for (cnt = 0; cnt < card->num_links; cnt++) {
		dai_link = &card->dai_link[cnt];
		dai_link->codecs->of_node = codec_node;
		dai_link->platforms->of_node = platform_node;
	}

	return 0;
}
#else
static int dbmdx_init_dai_link(struct snd_soc_card *card)
{
	int cnt;
	struct snd_soc_dai_link *dai_link;

	for (cnt = 0; cnt < card->num_links; cnt++) {
		dai_link = &card->dai_link[cnt];
		dai_link->codecs->name = "dbmdx-codec.0";
		dai_link->platforms->name = "dbmdx-snd-soc-platform.0";
	}

	return 0;
}
#endif

static int dbmdx_snd_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct snd_soc_card *card = &dspg_dbmdx_card;

#if defined(DBMDX_DEFER_SND_CARD_LOADING)
	struct device_node *np = pdev->dev.of_node;
	int card_index = 0;
	struct snd_card *card_ref;
#endif

	/* struct device_node *np = pdev->dev.of_node; */

	/* note: platform_set_drvdata() here saves pointer to the card's data
	 * on the device's 'struct device_private *p'->driver_data
	 */

	dev_info(&pdev->dev, "%s:\n", __func__);

#if defined(DBMDX_DEFER_SND_CARD_LOADING)
	ret = of_property_read_u32(np, "wait_for_card_index",
			&card_index);
	if ((ret && ret != -EINVAL)) {
		dev_info(&pdev->dev, "%s: invalid 'card_index' using default\n",
				__func__);
	} else {
		dev_info(&pdev->dev, "%s: 'wait_for_card_index' = %d\n",
				__func__, card_index);
	}

	card_ref = snd_card_ref(card_index);
	if ((!card_ref) || !(card_ref->id[0])) {
		dev_info(&pdev->dev,
				"%s: Defering DBMDX SND card probe, wait card[%d]..\n",
				__func__, card_index);
		return -EPROBE_DEFER;
	}
#endif

	card->dev = &pdev->dev;
	if (dbmdx_init_dai_link(card) < 0) {
		dev_err(&pdev->dev, "%s: Initialization of DAI links failed\n",
			__func__);
		ret = -1;
		goto ERR_CLEAR;
	}

	/* Register ASoC sound Card */
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: registering of sound card failed: %d\n",
			__func__, ret);
		goto ERR_CLEAR;
	}
	dev_info(&pdev->dev, "%s: DBMDX ASoC card registered\n", __func__);

	return 0;

ERR_CLEAR:
	return ret;
}

static int dbmdx_snd_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	dev_info(&pdev->dev, "%s: DBMDX ASoC card unregistered\n", __func__);

	return 0;
}

static const struct of_device_id snd_dbmdx_of_ids[] = {
	{ .compatible = "dspg,snd-dbmdx-mach-drv" },
	{ },
};

static struct platform_driver board_dbmdx_snd_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = snd_dbmdx_of_ids,
		.pm = &snd_soc_pm_ops,
	},
	.probe = dbmdx_snd_probe,
	.remove = dbmdx_snd_remove,
};

#if !IS_MODULE(CONFIG_SND_SOC_DBMDX)
static int __init board_dbmdx_mod_init(void)
{
	return platform_driver_register(&board_dbmdx_snd_drv);
}
module_init(board_dbmdx_mod_init);

static void __exit board_dbmdx_mod_exit(void)
{
	platform_driver_unregister(&board_dbmdx_snd_drv);
}
module_exit(board_dbmdx_mod_exit);
#else
int board_dbmdx_snd_init(void)
{
	return platform_driver_register(&board_dbmdx_snd_drv);
}

void board_dbmdx_snd_exit(void)
{
	platform_driver_unregister(&board_dbmdx_snd_drv);
}
#endif

MODULE_DESCRIPTION("ASoC machine driver for DSPG DBMDX");
MODULE_AUTHOR("DSP Group");
MODULE_LICENSE("GPL");
