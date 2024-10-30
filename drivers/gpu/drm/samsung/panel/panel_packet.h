/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_PACKET_H__
#define __PANEL_PACKET_H__

#include "maptbl.h"
#include "panel_obj.h"

struct pkt_update_info;

/* packet type */
enum {
	/* tx packet */
	CMD_TYPE_TX_PKT_START,
	CMD_PKT_TYPE_NONE = CMD_TYPE_TX_PKT_START,
	DSI_PKT_TYPE_WR,
	DSI_PKT_TYPE_WR_COMP,
	DSI_PKT_TYPE_WR_PPS,
	DSI_PKT_TYPE_WR_SR, /* write to side-ram */
	DSI_PKT_TYPE_WR_SR_FAST, /* write to side-ram with busy-wait */
	DSI_PKT_TYPE_WR_MEM,
	CMD_TYPE_TX_PKT_END = DSI_PKT_TYPE_WR_MEM,

	/* rx packet */
	CMD_TYPE_RX_PKT_START,
	SPI_PKT_TYPE_RD = CMD_TYPE_RX_PKT_START,
	DSI_PKT_TYPE_RD_POC,
	DSI_PKT_TYPE_RD,
	CMD_TYPE_RX_PKT_END = DSI_PKT_TYPE_RD,

	/* spi packet */
	SPI_PKT_TYPE_WR,
	SPI_PKT_TYPE_SETPARAM,
	/* i2c packet */
	CMD_TYPE_I2C_START,
	I2C_PKT_TYPE_WR = CMD_TYPE_I2C_START,
	I2C_PKT_TYPE_RD,
	CMD_TYPE_I2C_END = I2C_PKT_TYPE_RD,
	MAX_CMD_PACKET_TYPE,
};

#define IS_TX_PKT_TYPE(_pkt_type_) \
	(((_pkt_type_) >= CMD_TYPE_TX_PKT_START && \
	  (_pkt_type_) <= CMD_TYPE_TX_PKT_END) || \
	  (_pkt_type_) == SPI_PKT_TYPE_WR || \
	  (_pkt_type_) == SPI_PKT_TYPE_SETPARAM || \
	  (_pkt_type_) == I2C_PKT_TYPE_WR)

#define IS_RX_PKT_TYPE(_pkt_type_) \
	(((_pkt_type_) >= CMD_TYPE_RX_PKT_START && \
	  (_pkt_type_) <= CMD_TYPE_RX_PKT_END) || \
	  (_pkt_type_) == I2C_PKT_TYPE_RD)

enum {
	PKT_OPTION_NONE = 0,
	PKT_OPTION_CHECK_TX_DONE = (1 << 0),
	PKT_OPTION_SR_ALIGN_4 = (1 << 1),
	PKT_OPTION_SR_ALIGN_12 = (1 << 2),
	PKT_OPTION_SR_ALIGN_16 = (1 << 3),
	PKT_OPTION_SR_REG_6C = (1 << 4),
	PKT_OPTION_SR_MAX_512 = (1 << 5),
	PKT_OPTION_SR_MAX_1024 = (1 << 6),
	PKT_OPTION_SR_MAX_2048 = (1 << 7),
};

struct panel_tx_msg {
	u32 gpara_offset;
	u32 len;
	u8 *buf;
};

struct panel_rx_msg {
	u32 addr;
	u32 gpara_offset;
	u32 len;
	u8 *buf;
};

struct pktinfo {
	struct pnobj base;
	u32 type;
	u32 addr;
	u8 *initdata;
	u8 *txbuf;
	u32 offset;
	u32 dlen;
	struct pkt_update_info *pktui;
	u32 nr_pktui;
	u32 option;
};

struct pkt_update_info {
	u32 offset;
	struct maptbl *maptbl;
};

struct rdinfo {
	struct pnobj base;
	u32 type;
	u32 addr;
	u32 offset;
	u32 len;
	u8 *data;
};

#define PKTUI(_name_) PN_CONCAT(pktui, _name_)
#define PKTINFO(_name_) PN_CONCAT(pkt, _name_)

#define DECLARE_PKTUI(_name_) \
struct pkt_update_info PKTUI(_name_)[]

#define DEFINE_PKTUI(_name_, _maptbl_, _update_offset_)	\
struct pkt_update_info PKTUI(_name_)[] =			\
{													\
	{												\
		.offset = (_update_offset_),				\
		.maptbl = (_maptbl_),						\
	},												\
}

#define PKTINFO_INIT(_name_, _type_, _arr_, _ofs_, _option_, _pktui_, _nr_pktui_) \
	{ .base = __PNOBJ_INITIALIZER(_name_, CMD_TYPE_TX_PACKET) \
	, .type = (_type_) \
	, .initdata = (_arr_) \
	, .txbuf = (_arr_) \
	, .offset = (_ofs_) \
	, .dlen = ARRAY_SIZE((_arr_)) \
	, .pktui = (_pktui_) \
	, .nr_pktui = (_nr_pktui_) \
	, .option = (_option_) }

#define DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, _option_, _pktui_, _nr_pktui_)	\
struct pktinfo PKTINFO(_name_) = PKTINFO_INIT(_name_, _type_, _arr_, _ofs_, _option_, _pktui_, _nr_pktui_)

#define DEFINE_VARIABLE_PACKET(_name_, _type_, _arr_, _ofs_)				\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, 0, PKTUI(_name_), ARRAY_SIZE(PKTUI(_name_)))

#define DEFINE_VARIABLE_PACKET_WITH_OPTION(_name_, _type_, _arr_, _ofs_, _option_)				\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, _option_, PKTUI(_name_), ARRAY_SIZE(PKTUI(_name_)))

#define DEFINE_STATIC_PACKET(_name_, _type_, _arr_, _ofs_)		\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, 0, (NULL), (0))

#define DEFINE_STATIC_PACKET_WITH_OPTION(_name_, _type_, _arr_, _ofs_, _option_)		\
DEFINE_PACKET(_name_, _type_, _arr_, _ofs_, _option_, (NULL), (0))

#define PN_MAP_DATA_TABLE(_name_)	PN_CONCAT(PN_CONCAT(__pn_name__, _name_), table)
#define PN_CMD_DATA_TABLE(_name_)	PN_CONCAT(__PN_NAME__, _name_)

#define DEFINE_PN_STATIC_PACKET(_name_, _type_, _arr_)			\
DEFINE_PACKET(PN_CONCAT(__pn_name__, _name_), _type_,	\
		PN_CONCAT(__PN_NAME__, _arr_), _ofs_, 0, NULL, 0)

#define RDINFO(_name_) PN_CONCAT(rd, _name_)

#define RDINFO_INIT(_rdiname, _type, _addr, _offset, _len)	\
	{ .base = __PNOBJ_INITIALIZER(_rdiname, CMD_TYPE_RX_PACKET) \
	, .type = (_type) \
	, .addr = (_addr) \
	, .offset = (_offset) \
	, .len = (_len) }

#define DEFINE_RDINFO(_name_, _type_, _addr_, _offset_, _len_)	\
struct rdinfo RDINFO(_name_) = RDINFO_INIT(_name_, _type_, _addr_, _offset_, _len_)

const char *packet_type_to_string(u32 type);
int string_to_packet_type(const char *str);
unsigned int get_pktinfo_type(struct pktinfo *pkt);
char *get_pktinfo_name(struct pktinfo *pkt);
u8 *get_pktinfo_initdata(struct pktinfo *pkt);
u8 *get_pktinfo_txbuf(struct pktinfo *pkt);
unsigned int get_rdinfo_type(struct rdinfo *rdi);
char *get_rdinfo_name(struct rdinfo *rdi);
bool is_tx_packet(struct pnobj *pnobj);
bool is_valid_tx_packet(struct pktinfo *tx_packet);
bool is_variable_packet(struct pktinfo *tx_packet);
bool is_valid_rdinfo(struct rdinfo *rdi);
struct pktinfo *create_tx_packet(char *name, u32 type,
		struct panel_tx_msg *msg, struct pkt_update_info *pktui, u32 nr_pktui, u32 option);
void destroy_tx_packet(struct pktinfo *tx_packet);
struct rdinfo *create_rx_packet(char *name, u32 type, struct panel_rx_msg *msg);
void destroy_rx_packet(struct rdinfo *rx_packet);
int update_tx_packet(struct pktinfo *info);
void *copy_pktinfo_data(void *dest, struct pktinfo *pkt);
int add_tx_packet_on_pnobj_refs(struct pktinfo *pkt, struct pnobj_refs *pnobj_refs);
void print_pktinfo(struct pktinfo *pkt, int index);
struct keyinfo *create_key_packet(char *name, unsigned int level,
		unsigned int key_type, struct pktinfo *pkt);
struct keyinfo *duplicate_key_packet(struct keyinfo *key);
void destroy_key_packet(struct keyinfo *key);
#endif /* __PANEL_PACKET_H__ */

