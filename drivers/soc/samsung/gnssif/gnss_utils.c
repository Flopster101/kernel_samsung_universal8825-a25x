// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "gnss_prj.h"
#include "gnss_utils.h"
#include "gnssif_version.h"

#if defined(DEBUG_GNSS_IPC_PKT)
static const char *hex = "0123456789abcdef";

/* dump2hex
 * dump data to hex as fast as possible.
 * the length of @buff must be greater than "@len * 3"
 * it need 3 bytes per one data byte to print.
 */
static inline int dump2hex(char *buff, const char *data, size_t len)
{
	char *dest = buff;
	int i;

	for (i = 0; i < len; i++) {
		*dest++ = hex[(data[i] >> 4) & 0xf];
		*dest++ = hex[data[i] & 0xf];
		*dest++ = ' ';
	}
	if (likely(len > 0))
		dest--; /* last space will be overwrited with null */

	*dest = '\0';

	return dest - buff;
}

static inline void pr_ipc_msg(int level, u8 ch, const char *prefix,
				const u8 *msg, unsigned int len)
{
	size_t offset;
	char str[MAX_STR_LEN] = {0, };

	if (prefix)
		snprintf(str, MAX_STR_LEN, "%s", prefix);

	offset = strlen(str);
	dump2hex((str + offset), msg, (len > MAX_HEX_LEN ? MAX_HEX_LEN : len));

	gif_info("%s\n", str);
}

void gnss_log_ipc_pkt(struct sk_buff *skb, enum direction dir)
{
	struct io_device *iod;
	struct link_device *ld;
	char prefix[MAX_PREFIX_LEN] = {0, };
	unsigned int hdr_len;
	unsigned int msg_len;
	u8 *msg;
	u8 *hdr;
	u8 ch;

	iod = skbpriv(skb)->iod;
	ld = skbpriv(skb)->ld;
	ch = skbpriv(skb)->exynos_ch;

	/**
	 * Make a string of the route
	 */
	snprintf(prefix, MAX_PREFIX_LEN, "%s %s: %s: ",
		"LNK", dir_str(dir), ld->name);

	hdr = skbpriv(skb)->lnk_hdr ? skb->data : NULL;
	hdr_len = hdr ? EXYNOS_HEADER_SIZE : 0;
	if (hdr_len > 0) {
		char *separation = " | ";
		size_t offset = strlen(prefix);

		dump2hex((prefix + offset), hdr, hdr_len);
		strncat(prefix, separation, strlen(separation));
	}

	/* Print an IPC message with the prefix */
	msg = skb->data + hdr_len;
	msg_len = (skb->len - hdr_len);

	pr_ipc_msg(log_info.fmt_msg, ch, prefix, msg, msg_len);
}
#endif

const char *get_gnssif_driver_version(void)
{
	return &(gnssif_driver_version[0]);
}

void gif_init_irq(struct gnss_irq *irq, unsigned int num, const char *name,
	unsigned long flags)
{
	spin_lock_init(&irq->lock);
	irq->num = num;
	strncpy(irq->name, name, sizeof(irq->name) - 1);
	irq->flags = flags;
	gif_info("name:%s num:%d flags:0x%08lX\n", name, num, flags);
}

int gif_request_irq(struct gnss_irq *irq, irq_handler_t isr, void *data)
{
	int ret;

	ret = request_irq(irq->num, isr, irq->flags, irq->name, data);
	if (ret) {
		gif_err("%s: ERR! request_irq fail (%d)\n", irq->name, ret);
		return ret;
	}

	enable_irq_wake(irq->num);
	irq->active = true;
	irq->registered = true;

	gif_info("%s(#%d) handler registered (flags:0x%08lX)\n",
		irq->name, irq->num, irq->flags);

	return 0;
}

void gif_enable_irq(struct gnss_irq *irq)
{
	unsigned long flags;

	spin_lock_irqsave(&irq->lock, flags);

	if (irq->active) {
		gif_err("%s(#%d) is already active <%ps>\n",
			irq->name, irq->num, CALLER);
		goto exit;
	}

	enable_irq(irq->num);
	enable_irq_wake(irq->num);

	irq->active = true;

	gif_info("%s(#%d) is enabled <%ps>\n",
		irq->name, irq->num, CALLER);

exit:
	spin_unlock_irqrestore(&irq->lock, flags);
}

void gif_disable_irq_nosync(struct gnss_irq *irq)
{
	unsigned long flags;

	if (irq->registered == false)
		return;

	spin_lock_irqsave(&irq->lock, flags);

	if (!irq->active) {
		gif_err("%s(#%d) is not active <%ps>\n",
			irq->name, irq->num, CALLER);
		goto exit;
	}

	disable_irq_nosync(irq->num);
	disable_irq_wake(irq->num);

	irq->active = false;

	gif_info("%s(#%d) is disabled <%ps>\n",
			irq->name, irq->num, CALLER);

exit:
	spin_unlock_irqrestore(&irq->lock, flags);
}

void gif_disable_irq_sync(struct gnss_irq *irq)
{
	if (irq->registered == false)
		return;

	spin_lock(&irq->lock);

	if (!irq->active) {
		spin_unlock(&irq->lock);
		gif_err("%s(#%d) is not active <%ps>\n",
				irq->name, irq->num, CALLER);
		return;
	}

	spin_unlock(&irq->lock);

	disable_irq(irq->num);
	disable_irq_wake(irq->num);

	spin_lock(&irq->lock);
	irq->active = false;
	spin_unlock(&irq->lock);

	gif_info("%s(#%d) is disabled <%ps>\n",
			irq->name, irq->num, CALLER);
}

int gif_gpio_get_value(unsigned int gpio, bool log_print)
{
	int value;

	if (!gpio_is_valid(gpio)) {
		gif_err("GET GPIO %d is failed\n", gpio);
		return -EINVAL;
	}

	value = gpio_get_value(gpio);

	if (log_print)
		gif_info("GET GPIO %d = %d\n", gpio, value);

	return value;
}
