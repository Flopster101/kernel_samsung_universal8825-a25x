/*
 * Copyright (C) 2016 Google, Inc.
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/gpio.h>

#include "main.h"
#include "comms.h"

#include "chub.h"

#define READ_ACK_TIMEOUT_MS	10
#define READ_MSG_TIMEOUT_MS	70

#define RESEND_SHORT_DELAY_US   500     /* 500us - 1ms */
#define RESEND_LONG_DELAY_US    100000  /* 100ms - 200ms */

static const uint32_t crc_table[] = {
	0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
	0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
	0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
	0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD
};

static uint32_t crc32_word(uint32_t crc, uint32_t data, int cnt)
{
	int i;
	crc = crc ^ data;

	for (i = 0; i < cnt; i++)
		crc = (crc << 4) ^ crc_table[crc >> 28];

	return crc;
}

uint32_t crc32(const uint8_t *buffer, int length, uint32_t crc)
{
	uint32_t *data = (uint32_t *)buffer;
	uint32_t word;
	int i;

	/* word by word crc32 */
	for (i = 0; i < (length >> 2); i++)
		crc = crc32_word(crc, data[i], 8);

	/* zero pad last word if required */
	if (length & 0x3) {
		for (i *= 4, word = 0; i < length; i++)
			word |= buffer[i] << ((i & 0x3) * 8);
		crc = crc32_word(crc, word, 8);
	}

	return crc;
}

static inline size_t pad(size_t length)
{
	return (length + 3) & ~3;
}

static inline size_t tot_len(size_t length)
{
	/* [TYPE:1] [LENGTH:3] [DATA] [PAD:0-3] [CRC:4] */
	return sizeof(uint32_t) + pad(length) + sizeof(uint32_t);
}

static struct nanohub_packet_pad *packet_alloc(int flags)
{
	int len =
	    sizeof(struct nanohub_packet_pad) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	uint8_t *packet = kmalloc(len, flags);
	memset(packet, 0xFF, len);
	return (struct nanohub_packet_pad *)packet;
}

#ifdef PACKET_LOW_DEBUG
enum comms_action {
	ca_tx,
	ca_rx_ack,
	ca_rx,
};

#define GET_ACT_STRING(act)	\
	((act) == ca_tx ? 'W' : ((act) == ca_rx_ack ? 'A' : 'R'))

/* This function is from hostIntfGetFooter function on nanohub kernel. */
static inline struct nanohub_packet_crc *get_footer(struct nanohub_packet *packet)
{
	return (void *)(packet + sizeof(*packet) + packet->len);
}

static inline void packet_disassemble(void *buf, int ret, enum comms_action act)
{
	struct nanohub_packet *packet = (struct nanohub_packet *)buf;
	struct nanohub_packet_crc *footer = get_footer(packet);

	DEBUG_PRINT(KERN_DEBUG,
		"%c-PACKET(ret:%d):buf:%p,sync:0x%x,seq:0x%x,reason:0x%x,len:%d,crc:0x%x\n",
		GET_ACT_STRING(act), ret, (unsigned long)buf,
		(unsigned int)packet->sync,	(unsigned int)packet->seq,
		(unsigned int)packet->reason, (unsigned int)packet->len,
		(unsigned int)footer->crc);
}
#else
#define packet_disassemble(a, b, c) do {} while (0)
#endif

static int packet_create(struct nanohub_packet *packet, uint32_t seq,
			 uint32_t reason, uint8_t len, const uint8_t *data,
			 bool user)
{
	struct nanohub_packet_crc crc;
	int ret = sizeof(struct nanohub_packet) + len +
	    sizeof(struct nanohub_packet_crc);

	if (packet) {
		packet->sync = COMMS_SYNC;
		packet->seq = seq;
		packet->reason = reason;
		packet->len = len;
		if (len > 0) {
			if (user) {
				if (copy_from_user(packet->data, data, len) !=
				    0)
					ret = ERROR_NACK;
			} else {
				memcpy(packet->data, data, len);
			}
		}
		crc.crc =
		    crc32((uint8_t *) packet,
			  sizeof(struct nanohub_packet) + len, ~0);
		memcpy(&packet->data[len], &crc.crc,
		       sizeof(struct nanohub_packet_crc));
	} else {
		ret = ERROR_NACK;
	}
	packet_disassemble(packet, ret, ca_tx);

	return ret;
}

static int packet_verify(struct nanohub_packet *packet)
{
	struct nanohub_packet_crc crc;
	int cmp;

	crc.crc =
	    crc32((uint8_t *) packet,
		  sizeof(struct nanohub_packet) + packet->len, ~0);

	cmp =
	    memcmp(&crc.crc, &packet->data[packet->len],
		   sizeof(struct nanohub_packet_crc));

	if (cmp != 0) {
		uint8_t *ptr = (uint8_t *)packet;
		nanohub_debug("nanohub: gen crc: %08x, got crc: %08x\n", crc.crc,
			 *(uint32_t *)&packet->data[packet->len]);
		nanohub_debug(
		    "nanohub: %02x [%02x %02x %02x %02x] [%02x %02x %02x %02x] [%02x] [%02x %02x %02x %02x\n",
		    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6],
		    ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12],
		    ptr[13]);
	}

	return cmp;
}

static void packet_free(struct nanohub_packet_pad *packet)
{
	kfree(packet);
}

static int read_ack(struct nanohub_data *data, struct nanohub_packet *response,
		    int timeout)
{
	int ret, i;
	const int max_size = sizeof(struct nanohub_packet) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	unsigned long end = jiffies + msecs_to_jiffies(READ_ACK_TIMEOUT_MS);

	for (i = 0; time_before_eq(jiffies, end); i++) {
		ret =
		    data->comms.read(data, (uint8_t *) response, max_size,
				     timeout);
		packet_disassemble(response, ret, ca_rx_ack);

		if (ret == 0) {
			nanohub_debug("nanohub: read_ack: %d: empty packet\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret < sizeof(struct nanohub_packet)) {
			nanohub_debug("nanohub: read_ack: %d: too small\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret <
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			nanohub_debug("nanohub: read_ack: %d: too small length\n",
				 i);
			ret = ERROR_NACK;
			continue;
		} else if (ret !=
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			nanohub_debug("nanohub: read_ack: %d: wrong length\n", i);
			ret = ERROR_NACK;
			break;
		} else if (packet_verify(response) != 0) {
			nanohub_debug("nanohub: read_ack: %d: invalid crc\n", i);
			ret = ERROR_NACK;
			break;
		} else {
			break;
		}
	}

	return ret;
}

static int read_msg(struct nanohub_data *data, struct nanohub_packet *response,
		    int timeout)
{
	int ret, i;
	const int max_size = sizeof(struct nanohub_packet) + MAX_UINT8 +
	    sizeof(struct nanohub_packet_crc);
	unsigned long end = jiffies + msecs_to_jiffies(READ_MSG_TIMEOUT_MS);

	for (i = 0; time_before_eq(jiffies, end); i++) {
		ret =
		    data->comms.read(data, (uint8_t *) response, max_size,
				     timeout);
		packet_disassemble(response, ret, ca_rx);

		if (ret == 0) {
			nanohub_debug("nanohub: read_msg: %d: empty packet\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret < sizeof(struct nanohub_packet)) {
			nanohub_debug("nanohub: read_msg: %d: too small\n", i);
			ret = ERROR_NACK;
			continue;
		} else if (ret <
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			nanohub_debug("nanohub: read_msg: %d: too small length\n",
				 i);
			ret = ERROR_NACK;
			continue;
		} else if (ret !=
			   sizeof(struct nanohub_packet) + response->len +
			   sizeof(struct nanohub_packet_crc)) {
			nanohub_debug("nanohub: read_msg: %d: wrong length\n", i);
			ret = ERROR_NACK;
			break;
		} else if (packet_verify(response) != 0) {
			nanohub_debug("nanohub: read_msg: %d: invalid crc\n", i);
			ret = ERROR_NACK;
			break;
		} else {
			break;
		}
	}

	return ret;
}

static inline void contexthub_update_result(struct nanohub_data *data, int err)
{
	struct contexthub_ipc_info *chub = data->pdata->mailbox_client;

	if (err >= 0)
		chub->err_cnt[CHUB_ERR_COMMS] = 0;
	else {
		chub->err_cnt[CHUB_ERR_COMMS]++;

		if (err == ERROR_NACK)
			chub->err_cnt[CHUB_ERR_COMMS_NACK]++;
		else if (err == ERROR_BUSY)
			chub->err_cnt[CHUB_ERR_COMMS_BUSY]++;
		else
			chub->err_cnt[CHUB_ERR_COMMS_UNKNOWN]++;
	}
}

static int get_reply(struct nanohub_data *data, struct nanohub_packet *response,
		     uint32_t seq)
{
	int ret;

	ret = read_ack(data, response, data->comms.timeout_ack);

	if (ret >= 0 && response->seq == seq) {
		if (response->reason == CMD_COMMS_ACK) {
			if (response->len == sizeof(data->interrupts))
				memcpy(data->interrupts, response->data,
				       response->len);
			ret = read_msg(data, response, data->comms.timeout_reply);
			if (ret < 0 || response->reason == CMD_COMMS_ACK)
				ret = ERROR_NACK;
		} else {
			int i;
			uint8_t *b = (uint8_t *) response;

			if ((response->reason == CMD_COMMS_READ) ||
			    (response->reason == CMD_COMMS_WRITE))
				return ret;
			for (i = 0; i < ret; i += 25)
				nanohub_debug(
				    "nanohub: %d: %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				    ret, i, b[i], b[i + 1], b[i + 2], b[i + 3],
				    b[i + 4], b[i + 5], b[i + 6], b[i + 7],
				    b[i + 8], b[i + 9], b[i + 10], b[i + 11],
				    b[i + 12], b[i + 13], b[i + 14], b[i + 15],
				    b[i + 16], b[i + 17], b[i + 18], b[i + 19],
				    b[i + 20], b[i + 21], b[i + 22], b[i + 23],
				    b[i + 24]);
			if (response->reason == CMD_COMMS_NACK)
				ret = ERROR_NACK;
			else if (response->reason == CMD_COMMS_BUSY)
				ret = ERROR_BUSY;
		}

		if (response->seq != seq)
			ret = ERROR_NACK;
	} else {
		if (ret >= 0) {
			int i;
			uint8_t *b = (uint8_t *) response;
			for (i = 0; i < ret; i += 25)
				nanohub_debug(
				    "nanohub: %d: %d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				    ret, i, b[i], b[i + 1], b[i + 2], b[i + 3],
				    b[i + 4], b[i + 5], b[i + 6], b[i + 7],
				    b[i + 8], b[i + 9], b[i + 10], b[i + 11],
				    b[i + 12], b[i + 13], b[i + 14], b[i + 15],
				    b[i + 16], b[i + 17], b[i + 18], b[i + 19],
				    b[i + 20], b[i + 21], b[i + 22], b[i + 23],
				    b[i + 24]);
		}
		ret = ERROR_NACK;
	}

	return ret;
}

static int nanohub_comms_tx_rx(struct nanohub_data *data,
			       struct nanohub_packet_pad *pad, int packet_size,
			       uint32_t seq, uint8_t *rx, size_t rx_len)
{
	int ret;

	ret = data->comms.write(data, (uint8_t *)&pad->packet, packet_size,
				data->comms.timeout_write);

	if (ret == packet_size) {
		ret = get_reply(data, &pad->packet, seq);

		if (ret >= 0) {
			if (pad->packet.len > 0) {
				if (pad->packet.len > rx_len) {
					memcpy(rx, pad->packet.data, rx_len);
					ret = rx_len;
				} else {
					memcpy(rx, pad->packet.data,
					       pad->packet.len);
					ret = pad->packet.len;
				}
			} else {
				ret = 0;
			}
		}
	} else {
		ret = ERROR_NACK;
	}

	contexthub_update_result(data, ret);
	return ret;
}

int nanohub_comms_tx_rx_retrans(struct nanohub_data *data, uint32_t cmd,
				const uint8_t *tx, uint8_t tx_len,
				uint8_t *rx, size_t rx_len, bool user,
				int retrans_cnt, int retrans_delay)
{
	int packet_size = 0;
	struct nanohub_packet_pad *pad = packet_alloc(GFP_KERNEL);
	int delay = 0;
	int ret;
	uint32_t seq;
	uint64_t boottime;

	if (pad == NULL)
		return ERROR_NACK;

	seq = data->comms.seq++;

	do {
		data->comms.open(data);
		if (tx)
			packet_size = packet_create(&pad->packet, seq, cmd, tx_len, tx, user);
		else {
			boottime = ktime_get_boottime_ns();
			packet_size = packet_create(&pad->packet, seq, cmd, sizeof(boottime),
						    (uint8_t *)&boottime, false);
		}

		ret = nanohub_comms_tx_rx(data, pad, packet_size, seq, rx, rx_len);

		if (nanohub_wakeup_eom(data,
				       (ret == ERROR_BUSY) ||
				       (ret == ERROR_NACK && retrans_cnt >= 0)))
			ret = -EFAULT;

		data->comms.close(data);

		if (ret == ERROR_NACK) {
			retrans_cnt--;
			delay += retrans_delay;
			if (retrans_cnt >= 0)
				udelay(retrans_delay);
		} else if (ret == ERROR_BUSY) {
			usleep_range(RESEND_LONG_DELAY_US, RESEND_LONG_DELAY_US * 2);
		}
	} while ((ret == ERROR_BUSY) || (ret == ERROR_NACK && retrans_cnt >= 0));

	packet_free(pad);

	return ret;
}
