/* SPDX-License-Identifier: GPL-2.0 */
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
#include "panel_packet.h"

const char *packet_type_name[MAX_CMD_PACKET_TYPE] = {
	[CMD_PKT_TYPE_NONE] = "PKT_NONE",
	[SPI_PKT_TYPE_WR] = "SPI_WR",
	[DSI_PKT_TYPE_WR] = "DSI_WR",
	[DSI_PKT_TYPE_WR_COMP] = "DSI_WR_DSC",
	[DSI_PKT_TYPE_WR_PPS] = "DSI_WR_PPS",
	[DSI_PKT_TYPE_WR_SR] = "DSI_WR_SR",
	[DSI_PKT_TYPE_WR_SR_FAST] = "DSI_WR_SR_FAST",
	[DSI_PKT_TYPE_WR_MEM] = "DSI_WR_MEM",
	[SPI_PKT_TYPE_RD] = "SPI_RD",
	[DSI_PKT_TYPE_RD] = "DSI_RD",
#ifdef CONFIG_USDM_PANEL_DDI_FLASH
	[DSI_PKT_TYPE_RD_POC] = "DSI_RD_POC",
#endif
	[SPI_PKT_TYPE_SETPARAM] = "SPI_SETPARAM",
	[I2C_PKT_TYPE_WR] = "I2C_WR",
	[I2C_PKT_TYPE_RD] = "I2C_RD",
};

const char *packet_type_to_string(u32 type)
{
	return (type < MAX_CMD_PACKET_TYPE) ?
		packet_type_name[type] :
		packet_type_name[CMD_PKT_TYPE_NONE];
}

int string_to_packet_type(const char *str)
{
	unsigned int type;

	if (!str)
		return -EINVAL;

	for (type = 0; type < MAX_CMD_PACKET_TYPE; type++) {
		if (!packet_type_name[type])
			continue;

		if (!strcmp(packet_type_name[type], str))
			return type;
	}

	return -EINVAL;
}

unsigned int get_pktinfo_type(struct pktinfo *pkt)
{
	return pkt->type;
}

char *get_pktinfo_name(struct pktinfo *pkt)
{
	return get_pnobj_name(&pkt->base);
}

u8 *get_pktinfo_initdata(struct pktinfo *pkt)
{
	return pkt->initdata;
}

u8 *get_pktinfo_txbuf(struct pktinfo *pkt)
{
	return pkt->txbuf;
}

unsigned int get_rdinfo_type(struct rdinfo *rdi)
{
	return rdi->type;
}

char *get_rdinfo_name(struct rdinfo *rdi)
{
	return get_pnobj_name(&rdi->base);
}

bool is_tx_packet(struct pnobj *pnobj)
{
	unsigned int cmd_type;

	if (!pnobj)
		return false;

	cmd_type = get_pnobj_cmd_type(pnobj);
	if (!IS_CMD_TYPE_TX_PKT(cmd_type))
		return false;

	return true;
}

bool is_valid_tx_packet(struct pktinfo *tx_packet)
{
	unsigned int cmd_type;
	unsigned int pkt_type;

	if (!tx_packet)
		return false;

	cmd_type = get_pnobj_cmd_type(&tx_packet->base);
	if (!IS_CMD_TYPE_TX_PKT(cmd_type))
		return false;

	pkt_type = get_pktinfo_type(tx_packet);
	if (!IS_TX_PKT_TYPE(pkt_type))
		return false;

	return true;
}

bool is_variable_packet(struct pktinfo *tx_packet)
{
	if (!tx_packet)
		return false;

	return !!tx_packet->pktui;
}

bool is_valid_rdinfo(struct rdinfo *rdi)
{
	unsigned int cmd_type;
	unsigned int pkt_type;

	if (!rdi)
		return false;

	cmd_type = get_pnobj_cmd_type(&rdi->base);
	if (!IS_CMD_TYPE_RX_PKT(cmd_type))
		return false;

	pkt_type = get_rdinfo_type(rdi);
	if (!IS_RX_PKT_TYPE(pkt_type))
		return false;

	return true;
}

static int prepare_pktinfo_txbuf(struct pktinfo *pkt)
{
	u8 *txbuf;

	if (!pkt)
		return -EINVAL;

	if (!get_pktinfo_initdata(pkt))
		return -EINVAL;

	txbuf = get_pktinfo_txbuf(pkt);
	if (txbuf && txbuf != get_pktinfo_initdata(pkt)) {
		panel_err("%s txbuf already exist\n", get_pktinfo_name(pkt));
		return -EINVAL;
	}

	pkt->txbuf = kvmalloc(pkt->dlen, GFP_KERNEL);
	if (!pkt->txbuf)
		return -ENOMEM;

	memcpy(pkt->txbuf, pkt->initdata, pkt->dlen);

	return 0;
}

/**
 * create_tx_packet - create a struct pktinfo structure
 * @name: pointer to a string for the name of this packet.
 * @type: type of packet.
 * @msg: message to tx.
 * @pktui: pointer to a packet update information array.
 * @nr_pktui: number of packet update information.
 * @option: property of tx packet.
 *
 * This is used to create a struct pktinfo pointer.
 *
 * Returns &struct pktinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_tx_packet().
 */
struct pktinfo *create_tx_packet(char *name, u32 type,
		struct panel_tx_msg *msg, struct pkt_update_info *pktui, u32 nr_pktui, u32 option)
{
	struct pktinfo *tx_packet;
	int ret;

	if (!name)
		return NULL;

	if (!msg)
		return NULL;

	if (!IS_TX_PKT_TYPE(type))
		return NULL;

	tx_packet = kzalloc(sizeof(*tx_packet), GFP_KERNEL);
	if (!tx_packet)
		return NULL;

	tx_packet->type = type;
	tx_packet->initdata =
		kvmalloc(sizeof(u8) * msg->len, GFP_KERNEL);
	if (!tx_packet->initdata)
		goto err_alloc_data;

	tx_packet->offset = msg->gpara_offset;
	memcpy(tx_packet->initdata, msg->buf, msg->len);
	tx_packet->dlen = msg->len;
	tx_packet->txbuf = tx_packet->initdata;

	if (pktui && nr_pktui > 0) {
		tx_packet->pktui =
			kzalloc(sizeof(*pktui) * nr_pktui, GFP_KERNEL);
		if (!tx_packet->pktui)
			goto err_alloc_pktui;

		memcpy(tx_packet->pktui, pktui, sizeof(*pktui) * nr_pktui);
		tx_packet->nr_pktui = nr_pktui;

		ret = prepare_pktinfo_txbuf(tx_packet);
		if (ret < 0)
			goto err_prepare_txbuf;
	}
	tx_packet->option = option;
	pnobj_init(&tx_packet->base, CMD_TYPE_TX_PACKET, name);

	return tx_packet;

err_prepare_txbuf:
	kfree(tx_packet->pktui);

err_alloc_pktui:
	kvfree(tx_packet->initdata);

err_alloc_data:
	kfree(tx_packet);

	return NULL;
}
EXPORT_SYMBOL(create_tx_packet);

/**
 * destroy_tx_packet - destroys a struct pktinfo structure
 * @tx_packet: pointer to the struct pktinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_tx_packet().
 */
void destroy_tx_packet(struct pktinfo *tx_packet)
{
	if (!tx_packet)
		return;

	pnobj_deinit(&tx_packet->base);
	if (tx_packet->initdata != tx_packet->txbuf)
		kvfree(tx_packet->txbuf);
	kfree(tx_packet->pktui);
	kvfree(tx_packet->initdata);
	kfree(tx_packet);
}
EXPORT_SYMBOL(destroy_tx_packet);

/**
 * create_rx_packet - create a struct rdinfo structure
 * @name: pointer to a string for the name of this packet.
 * @type: type of packet.
 *
 * This is used to create a struct rdinfo pointer.
 *
 * Returns &struct rdinfo pointer on success, or NULL on error.
 *
 * Note, the pointer created here is to be destroyed when finished by
 * making a call to destroy_rx_packet().
 */
struct rdinfo *create_rx_packet(char *name, u32 type, struct panel_rx_msg *msg)
{
	struct rdinfo *rx_packet;

	if (!name)
		return NULL;

	if (!msg)
		return NULL;

	if (!msg->len) {
		panel_err("len is 0\n");
		return NULL;
	}

	if (!msg->addr) {
		panel_err("addr is 0\n");
		return NULL;
	}

	if (!IS_RX_PKT_TYPE(type))
		return NULL;

	rx_packet = kzalloc(sizeof(*rx_packet), GFP_KERNEL);
	if (!rx_packet)
		return NULL;

	rx_packet->type = type;
	rx_packet->data =
		kvmalloc(sizeof(u8) * msg->len, GFP_KERNEL);
	if (!rx_packet->data)
		goto err;

	rx_packet->addr = msg->addr;
	rx_packet->offset = msg->gpara_offset;
	rx_packet->len = msg->len;
	pnobj_init(&rx_packet->base, CMD_TYPE_RX_PACKET, name);

	return rx_packet;

err:
	kfree(rx_packet);
	return NULL;
}
EXPORT_SYMBOL(create_rx_packet);

/**
 * destroy_rx_packet - destroys a struct pktinfo structure
 * @rx_packet: pointer to the struct pktinfo that is to be destroyed
 *
 * Note, the pointer to be destroyed must have been created with a call
 * to create_rx_packet().
 */
void destroy_rx_packet(struct rdinfo *rx_packet)
{
	if (!rx_packet)
		return;

	pnobj_deinit(&rx_packet->base);
	kvfree(rx_packet->data);
	kfree(rx_packet);
}
EXPORT_SYMBOL(destroy_rx_packet);

int update_tx_packet_from_maptbl(struct pktinfo *pkt, struct pkt_update_info *pktui)
{
	struct maptbl *maptbl;
	int offset, ret;
	u8 *txbuf;

	txbuf = get_pktinfo_txbuf(pkt);
	if (!txbuf)
		return -EINVAL;

	offset = pktui->offset;
	maptbl = pktui->maptbl;
	if (!maptbl) {
		panel_err("%s invalid maptbl\n", get_pktinfo_name(pkt));
		return -EINVAL;
	}

	if (offset + maptbl_get_sizeof_copy(maptbl) > pkt->dlen) {
		panel_err("%s exceed size of buffer\n", get_pktinfo_name(pkt));
		return -EINVAL;
	}

	if (!maptbl_is_initialized(maptbl)) {
		ret = maptbl_init(maptbl);
		if (ret < 0) {
			panel_err("%s failed to initialize maptbl(%s)\n",
					get_pktinfo_name(pkt), maptbl_get_name(maptbl));
			return ret;
		}

		panel_info("%s initialize maptbl(%s)\n",
				get_pktinfo_name(pkt), maptbl_get_name(maptbl));
	}

	ret = maptbl_copy(maptbl, txbuf + offset);
	if (ret < 0) {
		panel_err("%s failed to copy maptbl(%s)\n",
				get_pktinfo_name(pkt), maptbl_get_name(maptbl));
		return ret;
	}

	panel_dbg("copy %d bytes from %s to %s[%d]\n",
			maptbl_get_sizeof_copy(maptbl), maptbl_get_name(maptbl),
			get_pktinfo_name(pkt), offset);

	return 0;
}

int update_tx_packet(struct pktinfo *pkt)
{
	int i, ret;

	if (!is_valid_tx_packet(pkt)) {
		panel_err("invalid packet\n");
		return -EINVAL;
	}

	if (!pkt->pktui || !pkt->nr_pktui) {
		panel_err("unable to update static packet\n");
		return -EINVAL;
	}

	for (i = 0; i < pkt->nr_pktui; i++) {
		ret = update_tx_packet_from_maptbl(pkt, &pkt->pktui[i]);
		if (ret < 0) {
			panel_err("%s failed to update from pktui[%d]",
					get_pktinfo_name(pkt), i);
			return ret;
		}
	}

	return 0;
}

void *copy_pktinfo_data(void *dest, struct pktinfo *pkt)
{
	u8 *txbuf = get_pktinfo_txbuf(pkt);
	
	if (!txbuf) {
		panel_err("txbuf is null\n");
		return NULL;
	}

	return memcpy(dest, txbuf, pkt->dlen);
}

int add_tx_packet_on_pnobj_refs(struct pktinfo *pkt, struct pnobj_refs *pnobj_refs)
{
	if (!pkt) {
		panel_err("packet is null\n");
		return -EINVAL;
	}

	if (!pnobj_refs) {
		panel_err("pnobj_refs is null\n");
		return -EINVAL;
	}

	return add_pnobj_ref(pnobj_refs, &pkt->base);
}

static int snprintf_pktinfo_with_index(char *buf, size_t size, struct pktinfo *pkt, int index)
{
	int len;
	u8 *txbuf = get_pktinfo_txbuf(pkt);
	
	if (!txbuf) {
		panel_err("txbuf is null\n");
		return 0;
	}

	if (index < 0)
		len = snprintf(buf, size, "pkt: %s: ",
				get_pktinfo_name(pkt));
	else
		len = snprintf(buf, size, "pkt: %s[%d]: ",
				get_pktinfo_name(pkt), index);

	len += usdm_snprintf_bytes(buf + len, size - len, txbuf, pkt->dlen);

	return len;
}

void print_pktinfo(struct pktinfo *pkt, int index)
{
	char buf[1024] = {0, };

	if (!pkt) {
		panel_err("packet is null\n");
		return;
	}

	snprintf_pktinfo_with_index(buf, ARRAY_SIZE(buf), pkt, index);
	panel_dbg("%s\n", buf);
}

struct keyinfo *create_key_packet(char *name, unsigned int level,
		unsigned int key_type, struct pktinfo *pkt)
{
	struct keyinfo *key;

	if (!name)
		return NULL;

	if (key_type >= MAX_KEY_TYPE) {
		panel_err("invalid key type(%d)\n", key_type);
		return NULL;
	}

	if (!pkt) {
		panel_err("packet is null\n");
		return NULL;
	}

	key = kzalloc(sizeof(*key), GFP_KERNEL);
	if (!key)
		return NULL;

	pnobj_init(&key->base, CMD_TYPE_KEY, name);
	key->level = level;
	key->en = key_type;
	key->packet = pkt;

	return key;
}

struct keyinfo *duplicate_key_packet(struct keyinfo *key)
{
	return create_key_packet(get_key_name(key),
			key->level, key->en, key->packet);
}

void destroy_key_packet(struct keyinfo *key)
{
	pnobj_deinit(&key->base);
	kfree(key);
}
