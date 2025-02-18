 /* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/ktime.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-adv-tracer-ipc.h>
#include <soc/samsung/exynos-pmu-if.h>

static struct adv_tracer_info *exynos_ati;
static struct adv_tracer_ipc_main *adv_tracer_ipc;

static void adv_tracer_ipc_read_buffer(void *dest, const void *src,
		unsigned int len)
{
	const unsigned int *sp = src;
	unsigned int *dp = dest;
	int i;

	for (i = 0; i < len; i++)
		*dp++ = readl(sp++);
}

static void adv_tracer_ipc_write_buffer(void *dest, const void *src,
		unsigned int len)
{
	const unsigned int *sp = src;
	unsigned int *dp = dest;
	int i;

	for (i = 0; i < len; i++)
		writel(*sp++, dp++);
}

static void adv_tracer_ipc_interrupt_clear(int id)
{
	__raw_writel((1 << INTR_FLAG_OFFSET) << id,
			adv_tracer_ipc->mailbox_base + AP_INTCR);
}

static irqreturn_t adv_tracer_ipc_irq_handler(int irq, void *data)
{
	struct adv_tracer_ipc_main *ipc = data;
	struct adv_tracer_ipc_cmd *cmd;
	struct adv_tracer_ipc_ch *channel;
	unsigned int status;
	int i;

	/* ADV_TRACER IPC INTERRUPT STATUS REGISTER */
	status = __raw_readl(adv_tracer_ipc->mailbox_base + AP_INTSR);
	status = status >> INTR_FLAG_OFFSET;

	for (i = 0; i < adv_tracer_ipc->num_channels; i++) {
		channel = &adv_tracer_ipc->channel[i];
		if (status & (0x1 << channel->id)) {
			cmd = channel->cmd;
			adv_tracer_ipc_read_buffer(cmd->buffer,
					channel->buff_regs, channel->len);
			/* ADV_TRACER IPC INTERRUPT PENDING CLEAR */
			adv_tracer_ipc_interrupt_clear(channel->id);
		}
	}
	ipc->mailbox_status = status;
	return IRQ_WAKE_THREAD;
}

static irqreturn_t adv_tracer_ipc_irq_handler_thread(int irq, void *data)
{
	struct adv_tracer_ipc_main *ipc = data;
	struct adv_tracer_ipc_cmd *cmd;
	struct adv_tracer_ipc_ch *channel;
	unsigned int status;
	int i;

	status = ipc->mailbox_status;

	for (i = 0; i < adv_tracer_ipc->num_channels; i++) {
		channel = &adv_tracer_ipc->channel[i];
		if (status & (1 << i)) {
			cmd = channel->cmd;
			if (cmd->cmd_raw.response)
				complete(&channel->wait);

			if (cmd->cmd_raw.one_way || cmd->cmd_raw.response) {
				/* HANDLE to Plugin handler */
				if (channel->ipc_callback)
					channel->ipc_callback(cmd,
							channel->len);
			}
		}
	}
	return IRQ_HANDLED;
}

static void adv_tracer_interrupt_gen(unsigned int id)
{
	writel(1 << id, adv_tracer_ipc->mailbox_base + INTGR_AP_TO_DBGC);
}

int adv_tracer_ipc_send_data(unsigned int id, struct adv_tracer_ipc_cmd *cmd)
{
	struct adv_tracer_ipc_ch *channel;
	int ret;

	if (id >= EAT_MAX_CHANNEL && IS_ERR(cmd))
		return -EIO;

	channel = &adv_tracer_ipc->channel[id];
	cmd->cmd_raw.manual_polling = 0;
	cmd->cmd_raw.one_way = 0;
	cmd->cmd_raw.response = 0;

	spin_lock(&channel->ch_lock);
	memcpy(channel->cmd, cmd, sizeof(unsigned int) * channel->len);
	adv_tracer_ipc_write_buffer(channel->buff_regs, channel->cmd,
			channel->len);
	adv_tracer_interrupt_gen(channel->id);
	spin_unlock(&channel->ch_lock);

	ret = wait_for_completion_interruptible_timeout(&channel->wait,
			msecs_to_jiffies(1000));
	if (!ret) {
		dev_err(exynos_ati->dev, "%d channel timeout error\n", id);
		return -EBUSY;
	}
	memcpy(cmd, channel->cmd, sizeof(unsigned int) * channel->len);
	return 0;
}
EXPORT_SYMBOL(adv_tracer_ipc_send_data);

int adv_tracer_ipc_send_data_polling_timeout(unsigned int id,
		struct adv_tracer_ipc_cmd *cmd, unsigned long tmout_ns)
{
	struct adv_tracer_ipc_ch *channel = NULL;
	u64 timeout, now;
	struct adv_tracer_ipc_cmd _cmd;

	if (id >= EAT_MAX_CHANNEL && IS_ERR(cmd))
		return -EIO;

	channel = &adv_tracer_ipc->channel[id];
	cmd->cmd_raw.manual_polling = 1;
	cmd->cmd_raw.one_way = 0;
	cmd->cmd_raw.response = 0;

	spin_lock(&channel->ch_lock);
	memcpy(channel->cmd, cmd, sizeof(unsigned int) * channel->len);
	adv_tracer_ipc_write_buffer(channel->buff_regs, channel->cmd,
			channel->len);
	adv_tracer_interrupt_gen(channel->id);
	spin_unlock(&channel->ch_lock);

	timeout = local_clock() + tmout_ns;
	now = local_clock();
	do {
		now = local_clock();
		adv_tracer_ipc_read_buffer(&_cmd.buffer[0],
				channel->buff_regs, 1);
	} while (!_cmd.cmd_raw.response && (timeout > now));

	if (!_cmd.cmd_raw.response) {
		dev_err(exynos_ati->dev, "%d channel timeout error\n", id);
		return -EBUSY;
	}
	adv_tracer_ipc_read_buffer(cmd->buffer, channel->buff_regs, channel->len);
	return 0;
}
EXPORT_SYMBOL(adv_tracer_ipc_send_data_polling_timeout);

int adv_tracer_ipc_send_data_polling(unsigned int id,
		struct adv_tracer_ipc_cmd *cmd)
{
	return adv_tracer_ipc_send_data_polling_timeout(id, cmd,
			EAT_IPC_TIMEOUT);
}
EXPORT_SYMBOL(adv_tracer_ipc_send_data_polling);

int adv_tracer_ipc_send_data_async(unsigned int id,
		struct adv_tracer_ipc_cmd *cmd)
{
	struct adv_tracer_ipc_ch *channel = NULL;

	if (id >= EAT_MAX_CHANNEL && IS_ERR(cmd))
		return -EIO;

	channel = &adv_tracer_ipc->channel[id];
	cmd->cmd_raw.manual_polling = 0;
	cmd->cmd_raw.one_way = 1;
	cmd->cmd_raw.response = 0;

	spin_lock(&channel->ch_lock);
	memcpy(channel->cmd, cmd, sizeof(unsigned int) * channel->len);
	adv_tracer_ipc_write_buffer(channel->buff_regs, channel->cmd,
			channel->len);
	adv_tracer_interrupt_gen(channel->id);
	spin_unlock(&channel->ch_lock);

	return 0;
}
EXPORT_SYMBOL(adv_tracer_ipc_send_data_async);

struct adv_tracer_ipc_cmd *adv_tracer_ipc_get_channel_cmd(unsigned int id)
{
	struct adv_tracer_ipc_ch *channel = NULL;
	struct adv_tracer_ipc_cmd *cmd = NULL;

	if (IS_ERR(adv_tracer_ipc))
		goto out;

	channel = &adv_tracer_ipc->channel[id];
	if (IS_ERR(channel))
		goto out;

	if (!channel->used)
		goto out;

	cmd = channel->cmd;
out:
	return cmd;
}
EXPORT_SYMBOL(adv_tracer_ipc_get_channel_cmd);

struct adv_tracer_ipc_ch *adv_tracer_ipc_get_channel(unsigned int id)
{
	struct adv_tracer_ipc_ch *ipc_channel = NULL;

	if (IS_ERR(adv_tracer_ipc))
		goto out;

	ipc_channel = &adv_tracer_ipc->channel[id];
	if (IS_ERR(ipc_channel)) {
		dev_err(exynos_ati->dev, "%d channel is not allocated\n", id);
		ipc_channel = NULL;
	}
out:
	return ipc_channel;
}
EXPORT_SYMBOL(adv_tracer_ipc_get_channel);

static int adv_tracer_ipc_channel_init(unsigned int id, unsigned int offset,
				       unsigned int len, ipc_callback handler,
				       char *name)
{
	struct adv_tracer_ipc_ch *channel = &adv_tracer_ipc->channel[id];

	if (channel->used) {
		dev_err(exynos_ati->dev, "%d channel is already reserved(%s)\n",
				id, channel->id_name);
		return -1;
	}
	channel->id = id;
	channel->offset = offset;
	channel->len = len;
	strncpy(channel->id_name, name, sizeof(unsigned int) - 1);
	channel->id_name[3] = '\0';

	/* channel->buff_regs -> shared buffer by owns */
	channel->buff_regs =
		(void __iomem *)(adv_tracer_ipc->mailbox_base + offset);
	channel->cmd = devm_kzalloc(adv_tracer_ipc->dev,
			sizeof(struct adv_tracer_ipc_cmd), GFP_KERNEL);
	channel->ipc_callback = handler;

	if (IS_ERR(channel->cmd))
		return PTR_ERR(channel->cmd);

	channel->used = true;
	return 0;
}

int adv_tracer_ipc_release_channel(unsigned int id)
{
	struct adv_tracer_ipc_cmd *cmd;
	int ret;

	if (!adv_tracer_ipc->channel[id].used) {
		dev_err(exynos_ati->dev, "%d channel is unused\n", id);
		return -1;
	}

	cmd = adv_tracer_ipc_get_channel_cmd(EAT_FRM_CHANNEL);
	if (!cmd) {
		dev_err(exynos_ati->dev, "%d channel is failed to release\n", id);
		return -EIO;
	}

	cmd->cmd_raw.cmd = EAT_IPC_CMD_CH_RELEASE;
	cmd->buffer[1] = id;

	ret = adv_tracer_ipc_send_data(EAT_FRM_CHANNEL, cmd);
	if (ret < 0) {
		dev_err(exynos_ati->dev, "%d channel is failed to release\n", id);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(adv_tracer_ipc_release_channel);

void adv_tracer_ipc_release_channel_by_name(const char *name)
{
	struct adv_tracer_ipc_ch *ipc_ch = adv_tracer_ipc->channel;
	int i;

	for (i = 0; i < adv_tracer_ipc->num_channels; i++) {
		if (ipc_ch[i].used) {
			if (!strncmp(name, ipc_ch[i].id_name, strlen(name))) {
				ipc_ch[i].used = 0;
				break;
			}
		}
	}
}
EXPORT_SYMBOL(adv_tracer_ipc_release_channel_by_name);

int adv_tracer_ipc_ramhold_ctrl(bool en, bool clear)
{
	struct adv_tracer_ipc_cmd cmd;
	int ret = 0;

	cmd.cmd_raw.cmd = EAT_IPC_CMD_RAMHOLD_CTRL;
	cmd.buffer[1] = en;
	cmd.buffer[2] = clear;
	ret = adv_tracer_ipc_send_data_polling_timeout(EAT_FRM_CHANNEL,
			&cmd, EAT_IPC_TIMEOUT);
	if (ret < 0)
		goto end;

	if (en != cmd.buffer[1])
		return -EINVAL;
end:
	return ret;
}
EXPORT_SYMBOL_GPL(adv_tracer_ipc_ramhold_ctrl);

int adv_tracer_ipc_request_channel(struct device_node *np,
		ipc_callback handler,
		unsigned int *id,
		unsigned int *len)
{
	const __be32 *prop;
	const char *plugin_name;
	unsigned int plugin_len, offset;
	struct adv_tracer_ipc_cmd cmd;
	int ret;

	if (!np)
		return -ENODEV;

	prop = of_get_property(np, "plugin-len", &plugin_len);
	if (!prop)
		return -ENOENT;

	plugin_len = be32_to_cpup(prop);
	plugin_name = of_get_property(np, "plugin-name", NULL);
	if (!plugin_name)
		return -ENOENT;

	cmd.cmd_raw.cmd = EAT_IPC_CMD_CH_INIT;
	cmd.buffer[1] = plugin_len;
	memcpy(&cmd.buffer[2], plugin_name, sizeof(unsigned int));

	ret = adv_tracer_ipc_send_data(EAT_FRM_CHANNEL, &cmd);
	if (ret) {
		dev_err(exynos_ati->dev, "%d channel is failed to request\n",
				EAT_FRM_CHANNEL);
		return -ENODEV;
	}

	*len = cmd.buffer[1];
	offset = SR(cmd.buffer[2]);
	*id = cmd.buffer[3];
	ret = adv_tracer_ipc_channel_init(*id, offset, *len, handler,
			(char *)plugin_name);
	if (ret) {
		dev_err(exynos_ati->dev,
				"%d channel is failed to init for %s(%d)\n",
				*id, plugin_name, ret);
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(adv_tracer_ipc_request_channel);

static void adv_tracer_ipc_channel_clear(void)
{
	struct adv_tracer_ipc_ch *channel;

	channel = &adv_tracer_ipc->channel[EAT_FRM_CHANNEL];
	channel->cmd->cmd_raw.cmd = EAT_IPC_CMD_CH_CLEAR;
	channel->cmd->cmd_raw.one_way = 1;
	adv_tracer_ipc_write_buffer(channel->buff_regs, channel->cmd, 1);
	adv_tracer_interrupt_gen(EAT_FRM_CHANNEL);
}

static void adv_tracer_ipc_callback(struct adv_tracer_ipc_cmd *cmd,
		unsigned int len)
{
	switch(cmd->cmd_raw.cmd) {
	case EAT_IPC_CMD_BOOT_DBGC:
		dev_info(adv_tracer_ipc->dev, "DBGC Boot!(cnt:%d)\n", cmd->buffer[1]);
		adv_tracer_ipc->exception_cnt--;
		break;
	case EAT_IPC_CMD_EXCEPTION_DBGC:
		dev_err(adv_tracer_ipc->dev,
				"DBGC occurred exception! (SP: 0x%x, LR:0x%x, PC:0x%x)\n",
				cmd->buffer[1], cmd->buffer[2], cmd->buffer[3]);
		adv_tracer_ipc->exception_cnt++;
		if (adv_tracer_ipc->exception_cnt >= 3)
			dbg_snapshot_expire_watchdog();
		break;
	default:
		break;
	}
}

static int adv_tracer_ipc_channel_probe(void)
{
	int i, ret;

	adv_tracer_ipc->num_channels = EAT_MAX_CHANNEL;
	adv_tracer_ipc->channel = devm_kcalloc(adv_tracer_ipc->dev,
			adv_tracer_ipc->num_channels,
			sizeof(struct adv_tracer_ipc_ch),
			GFP_KERNEL);

	if (IS_ERR(adv_tracer_ipc->channel))
		return PTR_ERR(adv_tracer_ipc->channel);

	for (i = 0; i < adv_tracer_ipc->num_channels; i++) {
		init_completion(&adv_tracer_ipc->channel[i].wait);
		spin_lock_init(&adv_tracer_ipc->channel[i].ch_lock);
	}

	ret = adv_tracer_ipc_channel_init(EAT_FRM_CHANNEL, SR(16), 4,
			adv_tracer_ipc_callback, FRAMEWORK_NAME);
	if (ret) {
		dev_err(exynos_ati->dev, "failed to register framework channel\n", ret);
		return ret;
	}

	adv_tracer_ipc_channel_clear();
	return 0;
}

static int adv_tracer_ipc_init(struct platform_device *pdev)
{
	int ret = 0;

	adv_tracer_ipc = devm_kzalloc(&pdev->dev,
			sizeof(struct adv_tracer_ipc_main), GFP_KERNEL);

	if (IS_ERR(adv_tracer_ipc))
		return PTR_ERR(adv_tracer_ipc);

	/* Mailbox interrupt, AP -- Debug Core */
	adv_tracer_ipc->irq = platform_get_irq(pdev, 0);

	/* Mailbox base register */
	adv_tracer_ipc->mailbox_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(adv_tracer_ipc->mailbox_base))
		return PTR_ERR(adv_tracer_ipc->mailbox_base);

	adv_tracer_ipc_interrupt_clear(EAT_FRM_CHANNEL);
	ret = devm_request_threaded_irq(&pdev->dev,
			adv_tracer_ipc->irq, adv_tracer_ipc_irq_handler,
			adv_tracer_ipc_irq_handler_thread,
			IRQF_ONESHOT,
			dev_name(&pdev->dev), adv_tracer_ipc);
	if (ret) {
		dev_err(&pdev->dev, "failed to register interrupt: %d\n", ret);
		return ret;
	}
	adv_tracer_ipc->dev = &pdev->dev;

	ret = adv_tracer_ipc_channel_probe();
	if (ret) {
		dev_err(&pdev->dev, "failed to register channel: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "%s successful.\n", __func__);
	return ret;
}

static int adv_tracer_probe(struct platform_device *pdev)
{
	struct adv_tracer_info *adv_tracer;
	int ret = 0;

	adv_tracer = devm_kzalloc(&pdev->dev,
			sizeof(struct adv_tracer_info), GFP_KERNEL);

	if (IS_ERR(adv_tracer))
		return PTR_ERR(adv_tracer);

	adv_tracer->dev = &pdev->dev;
	if (adv_tracer_ipc_init(pdev))
		goto out;

	exynos_ati = adv_tracer;

	dev_info(&pdev->dev, "%s successful.\n", __func__);
out:
	return ret;
}

static const struct of_device_id adv_tracer_match[] = {
	{ .compatible = "samsung,exynos-adv-tracer" },
	{},
};

static struct platform_driver exynos_adv_tracer_driver = {
	.probe	= adv_tracer_probe,
	.driver	= {
		.name = "exynos-adv-tracer",
		.owner	= THIS_MODULE,
		.of_match_table	= adv_tracer_match,
	},
};
module_platform_driver(exynos_adv_tracer_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Exynos Advanced Tracer");
