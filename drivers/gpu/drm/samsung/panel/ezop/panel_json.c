// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/list_sort.h>
#include "panel.h"
#include "panel_debug.h"
#include "maptbl.h"
#include "panel_drv.h"
#include "panel_json.h"
#include "panel_delay.h"
#include "panel_packet.h"
#include "panel_property.h"
#include "panel_config.h"
#include "panel_condition.h"
#include "panel_expression.h"
#include "panel_function.h"
#include "json.h"
#include "json_writer.h"
#include "json_reader.h"

const char *pnobj_cmd_type_to_json_path(u32 type)
{
	return cmd_type_to_string(type);
}

int jsonr_ref_field(json_reader_t *r, char *ref)
{
	int err;

	JEXPECT_STREQ(r, current_token(r), "$ref"); inc_token(r);

	err = jsonr_string(r, ref);
	if (err < 0)
		return err;

	return 0;

out_free:
	return err;
}

static int json_ref_to_pnobj(char *ref, struct pnobj *pnobj)
{
	char *p;
	int type;

	p = strsep(&ref, "/");
	if (!p || strcmp(p, "#"))
		return -EINVAL;

	p = strsep(&ref, "/");
	if (!p)
		return -EINVAL;

	type = string_to_cmd_type(p);
	if (type < 0)
		return -EINVAL;

	set_pnobj_cmd_type(pnobj, type);
	set_pnobj_name(pnobj, ref);

	return 0;
}

int jsonr_ref_pnobj(json_reader_t *r, struct pnobj *pnobj)
{
	jsmntok_t *obj_tok;
	int err;
	char name[SZ_128];
	char *ref = name;

	JEXPECT_OBJECT(r, current_token(r));
	obj_tok = current_token(r); inc_token(r);
	if (!obj_tok->size)
		return 0;

	err = jsonr_ref_field(r, ref);
	if (err < 0) {
		panel_err("failed to parse ref field(%s)\n", ref);
		return err;
	}

	err = json_ref_to_pnobj(ref, pnobj);
	if (err < 0) {
		panel_err("failed to ref to pnobj(%s)\n", ref);
		return err;
	}

	return 0;

out_free:
	return err;
}

int jsonr_ref_pnobj_object(json_reader_t *r, struct pnobj **pnobj)
{
	int err;
	struct pnobj t_pnobj, *found;

	memset(&t_pnobj, 0, sizeof(t_pnobj));
	err = jsonr_ref_pnobj(r, &t_pnobj);
	if (err < 0)
		goto out_free;

	if (!get_pnobj_name(&t_pnobj)) {
		*pnobj = NULL;
		return 0;
	}

	found = pnobj_find_by_pnobj(get_jsonr_pnobj_list(r), &t_pnobj);
	if (!found) {
		panel_err("pnobj(%s:%s) not found\n",
				get_pnobj_name(&t_pnobj),
				cmd_type_to_string(get_pnobj_cmd_type(&t_pnobj)));
		free_pnobj_name(&t_pnobj);
		return -EINVAL;
	}

	free_pnobj_name(&t_pnobj);
	*pnobj = found;

	return 0;

out_free:
	return err;

}

int jsonr_ref_pnobj_field(json_reader_t *r,
		const char *prop, struct pnobj **pnobj)
{
	int err;

	JEXPECT_STREQ(r, current_token(r), prop); inc_token(r);

	err = jsonr_ref_pnobj_object(r, pnobj);
	if (err < 0)
		return err;

out_free:
	return err;
}

int jsonr_ref_property_field(json_reader_t *r,
		const char *prop, struct panel_property **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct panel_property);

	return 0;
}

int jsonr_ref_function_field(json_reader_t *r,
		const char *prop, struct pnobj_func **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct pnobj_func);

	return 0;
}

int jsonr_ref_maptbl_field(json_reader_t *r,
		const char *prop, struct maptbl **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct maptbl);

	return 0;
}

int jsonr_ref_packet_field(json_reader_t *r,
		const char *prop, struct pktinfo **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct pktinfo);

	return 0;
}

int jsonr_ref_readinfo_field(json_reader_t *r,
		const char *prop, struct rdinfo **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct rdinfo);

	return 0;
}

int jsonr_ref_resource_field(json_reader_t *r,
		const char *prop, struct resinfo **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct resinfo);

	return 0;
}

int jsonr_ref_delay_field(json_reader_t *r,
		const char *prop, struct delayinfo **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct delayinfo);

	return 0;
}

int jsonr_ref_sequence_field(json_reader_t *r,
		const char *prop, struct seqinfo **out)
{
	struct pnobj *pnobj;
	int err;

	err = jsonr_ref_pnobj_field(r, prop, &pnobj);
	if (err < 0)
		return err;

	if (!pnobj) {
		*out = NULL;
		return 0;
	}

	*out = pnobj_container_of(pnobj, struct seqinfo);

	return 0;
}

int jsonr_pnobj(json_reader_t *r, struct pnobj *pnobj)
{
	struct pnobj temp_pnobj;
	char key[SZ_128];
	char value[SZ_128];
	char name[PNOBJ_NAME_LEN];
	int err, type;

	memset(&temp_pnobj, 0, sizeof(temp_pnobj));

	JEXPECT_STREQ(r, current_token(r), "pnobj"); inc_token(r);
	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);

	err = jsonr_string_field(r, key, value);
	if (err < 0)
		return err;

	if (strcmp(key, "type"))
		return -EINVAL;

	type = string_to_cmd_type(value);
	if (type < 0) {
		panel_err("invalid type(%s:%d)\n", value, type);
		return -EINVAL;
	}

	err = jsonr_string_field(r, key, name);
	if (err < 0)
		return err;

	if (strcmp(key, "name"))
		return -EINVAL;

	pnobj_init(pnobj, type, name);

	return 0;

out_free:

	return err;
}

int jsonr_pnobj_object(json_reader_t *r, struct pnobj *pnobj)
{
	int err;

	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);
	err = jsonr_pnobj(r, pnobj);
	if (err < 0)
		return err;

	return 0;

out_free:

	return err;
}

__visible_for_testing int jsonr_u8_rle(json_reader_t *r, u8 **out)
{
	jsmntok_t *it_tok;
	unsigned int *rle_data = NULL;
	unsigned int *rle_len = NULL;
	u8 *arr = NULL;
	int i, j, c = 0, err, num_unique_data;
	int total_size = 0;

	/* unique data */
	it_tok = current_token(r); inc_token(r);
	num_unique_data = it_tok->size;
	if (num_unique_data == 0) {
		panel_warn("rle_data is empty\n");
		goto out_free;
	}

	rle_data = kvmalloc(sizeof(unsigned int) * it_tok->size, GFP_KERNEL);
	err = jsonr_uint_array(r, rle_data, num_unique_data);
	if (err < 0)
		goto out_free;

	/* length of data */
	it_tok = current_token(r); inc_token(r);
	if (num_unique_data != it_tok->size) {
		panel_err("size mismatch(rle_data:%d != rle_len:%d)\n",
				num_unique_data, it_tok->size);
		goto out_free;
	}

	rle_len = kvmalloc(sizeof(unsigned int) * it_tok->size, GFP_KERNEL);
	err = jsonr_uint_array(r, rle_len, num_unique_data);
	if (err < 0)
		goto out_free;

	/* rle decoding */
	for (i = 0; i < num_unique_data; i++)
		total_size += rle_len[i];

	arr = kvmalloc(sizeof(u8) * total_size, GFP_KERNEL);
	for (i = 0; i < num_unique_data; i++) {
		for (j = 0; j < rle_len[i]; j++, c++)
			arr[c] = rle_data[i];
	}

	kvfree(rle_data);
	kvfree(rle_len);
	*out = arr;

	return total_size;

out_free:
	kvfree(rle_data);
	kvfree(rle_len);
	kvfree(arr);

	return 0;
}

__visible_for_testing int jsonr_byte_array_field(json_reader_t *r,
		const char *prop, u8 **out)
{
	jsmntok_t *it_tok;
	u8 *arr = NULL;
	int err, size;

	JEXPECT_STREQ(r, current_token(r), prop); inc_token(r);
	JEXPECT_ARRAY(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	if (it_tok->size == 0)
		return 0;

	/* RLE */
	if (JSMN_IS_ARRAY(current_token(r)) && it_tok->size == 2) {
		err = jsonr_u8_rle(r, &arr);
		if (err < 0)
			goto out_free;
		size = err;
	} else {
		arr = kvmalloc(sizeof(u8) * it_tok->size, GFP_KERNEL);
		err = jsonr_u8_array(r, arr, it_tok->size);
		if (err < 0)
			goto out_free;
		size = it_tok->size;
	}
	*out = arr;

	return size;

out_free:
	kvfree(arr);

	return err;
}

int jsonr_maptbl(json_reader_t *r, struct maptbl *m)
{
	jsmntok_t *it_tok;
	char buf[SZ_128];
	struct pnobj *pnobj;
	u8 *arr = NULL;
	int i, err;

	if (!m) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &m->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* parsing maptbl_shape */
	JEXPECT_STREQ(r, current_token(r), "shape"); inc_token(r);
	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);
	{
		err = jsonr_uint_field(r,
				buf, &m->shape.nr_dimen);
		if (err < 0)
			goto out_free;

		/* array */
		JEXPECT_STREQ(r, current_token(r), "sz_dimen"); inc_token(r);
		JEXPECT_ARRAY(r, current_token(r));
		it_tok = current_token(r); inc_token(r);
		err = jsonr_uint_array(r,
				m->shape.sz_dimen, it_tok->size);
		if (err < 0)
			goto out_free;
	}

	/* parsing maptbl array */
	err = jsonr_byte_array_field(r, "arr", &arr);
	if (err < 0)
		goto out_free;

	/* parsing maptbl_ops */
	JEXPECT_STREQ(r, current_token(r), "ops"); inc_token(r);
	JEXPECT_OBJECT(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	if (it_tok->size > 0) {
		err = jsonr_ref_function_field(r, "init", &m->ops.init);
		if (err < 0)
			goto out_free;

		err = jsonr_ref_function_field(r, "getidx", &m->ops.getidx);
		if (err < 0)
			goto out_free;

		err = jsonr_ref_function_field(r, "copy", &m->ops.copy);
		if (err < 0)
			goto out_free;
	}

	/* parsing dimension property */
	JEXPECT_STREQ(r, current_token(r), "props"); inc_token(r);
	JEXPECT_ARRAY(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	for (i = 0; i < it_tok->size; i++) {
		err = jsonr_ref_pnobj_object(r, &pnobj);
		if (err < 0)
			goto out_free;

		if (!pnobj) {
			panel_err("failed to get pnobj\n");
			goto out_free;
		}
		m->props.name[i] = get_pnobj_name(pnobj);
	}

	m->arr = arr;
	maptbl_set_sizeof_copy(m,
			maptbl_get_countof_n_dimen_element(m, NDARR_1D));

	return 0;

out_free:
	free_pnobj_name(&m->base);
	kvfree(arr);

	return err;
}

int jsonr_packet_update_info(json_reader_t *r, struct pkt_update_info *pktui)
{
	char buf[SZ_128];
	int err;
	struct pnobj_func *getidx;

	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);

	/* offset */
	err = jsonr_uint_field(r, buf, &pktui->offset);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		return err;
	}

	/* maptbl */
	err = jsonr_ref_maptbl_field(r, "maptbl", &pktui->maptbl);
	if (err < 0) {
		panel_err("failed to parse reference of maptbl\n");
		return err;
	}

	/* TODO: remove it when EasyOpcode Editor is fixed */
	/* getidx function */
	err = jsonr_ref_function_field(r, "getidx", &getidx);
	if (err < 0) {
		panel_err("failed to parse reference of function\n");
		return err;
	}

	return 0;

out_free:

	return err;
}

int jsonr_tx_packet(json_reader_t *r, struct pktinfo *pkt)
{
	jsmntok_t *it_tok;
	char key[SZ_128];
	char buf[SZ_128];
	struct pkt_update_info *pktui = NULL;
	u8 *txbuf = NULL;
	u8 *initdata = NULL;
	int i, err, pkt_type;

	if (!pkt) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &pkt->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* type */
	err = jsonr_string_field(r, key, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	pkt_type = string_to_packet_type(buf);
	if (pkt_type < 0 || !IS_TX_PKT_TYPE(pkt_type)) {
		panel_err("invalid type(%s:%d)\n", buf, pkt_type);
		err = -EINVAL;
		goto out_free;
	}
	pkt->type = pkt_type;

	/* parsing packet data */
	err = jsonr_byte_array_field(r, "data", &initdata);
	if (err < 0)
		goto out_free;
	pkt->dlen = err;

	/* offset */
	err = jsonr_uint_field(r, buf, &pkt->offset);
	if (err < 0)
		goto out_free;

	/* parsing maptbl array */
	JEXPECT_STREQ(r, current_token(r), "pktui"); inc_token(r);
	JEXPECT_ARRAY(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	if (it_tok->size > 0) {
		pktui = kzalloc(sizeof(*pktui) * it_tok->size, GFP_KERNEL);
		for (i = 0; i < it_tok->size; i++) {
			err = jsonr_packet_update_info(r, &pktui[i]);
			if (err < 0)
				goto out_free;
		}
		pkt->nr_pktui = it_tok->size;

		txbuf = kvmalloc(pkt->dlen, GFP_KERNEL);
		memcpy(txbuf, initdata, pkt->dlen);
	}

	/* option */
	err = jsonr_uint_field(r, buf, &pkt->option);
	if (err < 0)
		goto out_free;

	pkt->initdata = initdata;
	pkt->txbuf = txbuf ? txbuf : pkt->initdata;
	pkt->pktui = pktui;

	return 0;

out_free:
	free_pnobj_name(&pkt->base);
	kvfree(initdata);
	kvfree(txbuf);
	kfree(pktui);

	return err;
}

int jsonr_keyinfo(json_reader_t *r, struct keyinfo *key)
{
	char buf[SZ_128];
	int err;

	if (!key) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &key->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* level */
	err = jsonr_uint_field(r, buf, &key->level);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		goto out_free;
	}

	/* en */
	err = jsonr_uint_field(r, buf, &key->en);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		goto out_free;
	}

	/* packet */
	err = jsonr_ref_packet_field(r, "packet", &key->packet);
	if (err < 0) {
		panel_err("failed to parse reference of packet\n");
		goto out_free;
	}

	return 0;

out_free:
	free_pnobj_name(&key->base);
	return err;
}

int jsonr_panel_expr_data(json_reader_t *r, struct panel_expr_data *data)
{
	char buf[SZ_128];
	char str[SZ_128];
	int err;

	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);

	/* type */
	err = jsonr_string_field(r, buf, str);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	err = panel_expr_type_string_to_enum(str);
	if (err < 0) {
		panel_err("invalid expr type(%s)\n", str);
		return err;
	}
	data->type = err;

	if (data->type == PANEL_EXPR_TYPE_OPERAND_U32) {
		err = jsonr_uint_field(r, buf, &data->op.u32);
		if (err < 0) {
			panel_err("failed to parse uint\n");
			return err;
		}
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_S32) {
		err = jsonr_uint_field(r, buf, &data->op.s32);
		if (err < 0) {
			panel_err("failed to parse int\n");
			return err;
		}
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_STR) {
		err = jsonr_string_field(r, buf, str);
		if (err < 0) {
			panel_err("failed to parse string\n");
			return err;
		}
		data->op.str = kstrndup(str, sizeof(str)-1, GFP_KERNEL);
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_PROP) {
		err = jsonr_string_field(r, buf, str);
		if (err < 0) {
			panel_err("failed to parse string\n");
			return err;
		}
		data->op.str = kstrndup(str, sizeof(str)-1, GFP_KERNEL);
	} else if (data->type == PANEL_EXPR_TYPE_OPERAND_FUNC) {
		err = jsonr_ref_function_field(r, "op", &data->op.func);
		if (err < 0) {
			panel_err("failed to parse reference of function\n");
			return err;
		}
	}

	return 0;

out_free:
	return err;
}

int jsonr_condition(json_reader_t *r, struct condinfo *cond)
{
	char buf[SZ_128];
	int i, err;
	jsmntok_t *it_tok;
	struct panel_expr_data *item = NULL;

	if (!cond) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &cond->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* parsing rule */
	JEXPECT_STREQ(r, current_token(r), "rule"); inc_token(r);
	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);
	{
		/* parsing panel_expr_data array */
		JEXPECT_STREQ(r, current_token(r), "item"); inc_token(r);
		JEXPECT_ARRAY(r, current_token(r));
		it_tok = current_token(r); inc_token(r);
		if (it_tok->size > 0) {
			item = kzalloc(sizeof(*item) * it_tok->size, GFP_KERNEL);
			for (i = 0; i < it_tok->size; i++) {
				err = jsonr_panel_expr_data(r, &item[i]);
				if (err < 0)
					goto out_free;
			}
		}
		cond->rule.num_item = it_tok->size;
		cond->rule.item = item;
	}

	return 0;

out_free:
	kfree(item);
	free_pnobj_name(&cond->base);
	return err;
}

int jsonr_rx_packet(json_reader_t *r, struct rdinfo *rdi)
{
	char key[SZ_128];
	char buf[SZ_128];
	u8 *data = NULL;
	int err, pkt_type;

	if (!rdi) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &rdi->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* type */
	err = jsonr_string_field(r, key, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	pkt_type = string_to_packet_type(buf);
	if (pkt_type < 0 || !IS_RX_PKT_TYPE(pkt_type)) {
		panel_err("invalid type(%s:%d)\n", buf, pkt_type);
		err = -EINVAL;
		goto out_free;
	}
	rdi->type = pkt_type;

	/* address */
	err = jsonr_uint_field(r, buf, &rdi->addr);
	if (err < 0)
		goto out_free;

	/* offset */
	err = jsonr_uint_field(r, buf, &rdi->offset);
	if (err < 0)
		goto out_free;

	/* length */
	err = jsonr_uint_field(r, buf, &rdi->len);
	if (err < 0)
		goto out_free;

	data = kvmalloc(sizeof(u8) * rdi->len, GFP_KERNEL);
	if (!data)
		goto out_free;

	rdi->data = data;

	return 0;

out_free:
	free_pnobj_name(&rdi->base);
	kfree(data);
	return err;
}

int jsonr_resource_update_info(json_reader_t *r, struct res_update_info *resui)
{
	char buf[SZ_128];
	int err;

	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);

	/* offset */
	err = jsonr_uint_field(r, buf, &resui->offset);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		return err;
	}

	/* readinfo */
	err = jsonr_ref_readinfo_field(r, "rdi", &resui->rditbl);
	if (err < 0) {
		panel_err("failed to parse reference of readinfo\n");
		return err;
	}

	return 0;

out_free:
	return err;
}

int jsonr_resource(json_reader_t *r, struct resinfo *res)
{
	jsmntok_t *it_tok;
	char buf[SZ_128];
	u8 *data = NULL;
	struct res_update_info *resui = NULL;
	int i, err;

	if (!res) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &res->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* parsing packet data */
	err = jsonr_byte_array_field(r, "data", &data);
	if (err < 0)
		goto out_free;
	res->dlen = err;

	/* parsing resource update info */
	JEXPECT_STREQ(r, current_token(r), "resui"); inc_token(r);
	JEXPECT_ARRAY(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	if (it_tok->size > 0) {
		resui = kzalloc(sizeof(*resui) * it_tok->size, GFP_KERNEL);
		for (i = 0; i < it_tok->size; i++) {
			err = jsonr_resource_update_info(r, &resui[i]);
			if (err < 0)
				goto out_free;
		}
		res->nr_resui = it_tok->size;
	}

	res->data = data;
	res->resui = resui;
	if (!resui)
		res->state = RES_INITIALIZED;

	return 0;

out_free:
	free_pnobj_name(&res->base);
	kvfree(data);
	kfree(resui);

	return err;
}

int jsonr_dump_expect(json_reader_t *r, struct dump_expect *expect)
{
	char buf[SZ_128];
	char str[SZ_128];
	int err;

	JEXPECT_OBJECT(r, current_token(r)); inc_token(r);

	/* offset */
	err = jsonr_uint_field(r, buf, &expect->offset);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		return err;
	}

	/* mask */
	err = jsonr_u8_field(r, buf, &expect->mask);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		return err;
	}

	/* value */
	err = jsonr_u8_field(r, buf, &expect->value);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		return err;
	}

	/* msg */
	err = jsonr_string_field(r, buf, str);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		return err;
	}
	expect->msg = kstrndup(str, sizeof(str)-1, GFP_KERNEL);

	return 0;

out_free:
	return err;
}

int jsonr_dump_expect_array(json_reader_t *r,
		const char *prop, struct dump_expect **out, unsigned int *out_size)
{
	jsmntok_t *it_tok = NULL;
	int i, err;
	struct dump_expect *expects = NULL;

	if (!prop || !out)
		return -EINVAL;

	/* parsing expects array */
	JEXPECT_STREQ(r, current_token(r), prop); inc_token(r);
	JEXPECT_ARRAY(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	if (it_tok->size == 0) {
		*out = NULL;
		return 0;
	}

	expects = kzalloc(sizeof(*expects) * it_tok->size, GFP_KERNEL);
	for (i = 0; i < it_tok->size; i++) {
		err = jsonr_dump_expect(r, &expects[i]);
		if (err < 0)
			goto out_free;
	}
	*out = expects;
	*out_size = it_tok->size;

	return 0;

out_free:
	if (it_tok && expects) {
		for (i = 0; i < it_tok->size; i++)
			kfree(expects[i].msg);
	}
	kfree(expects);
	return err;
}

int jsonr_dumpinfo(json_reader_t *r, struct dumpinfo *dump)
{
	char buf[SZ_128];
	int err;

	if (!dump) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &dump->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* readinfo */
	err = jsonr_ref_resource_field(r, "res", &dump->res);
	if (err < 0) {
		panel_err("failed to parse reference\n");
		goto out_free;
	}

	/* callback function */
	err = jsonr_ref_function_field(r, "callback", &dump->ops.show);
	if (err < 0) {
		panel_err("failed to parse reference of function\n");
		goto out_free;
	}

	err = jsonr_dump_expect_array(r, "expects",
			&dump->expects, &dump->nr_expects);
	if (err < 0) {
		panel_err("failed to parse dump expects\n");
		goto out_free;
	}

	return 0;

out_free:
	free_pnobj_name(&dump->base);
	return err;
}

int jsonr_delayinfo(json_reader_t *r, struct delayinfo *delay)
{
	char buf[SZ_128];
	int err;

	if (!delay) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &delay->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* usec */
	err = jsonr_uint_field(r, buf, &delay->usec);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		goto out_free;
	}

	/* nframe */
	err = jsonr_uint_field(r, buf, &delay->nframe);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		goto out_free;
	}

	/* nvsync */
	err = jsonr_uint_field(r, buf, &delay->nvsync);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		goto out_free;
	}

	return 0;

out_free:
	free_pnobj_name(&delay->base);
	return err;
}

int jsonr_timer_delay_begin_info(json_reader_t *r, struct timer_delay_begin_info *tdbi)
{
	char buf[SZ_128];
	int err;

	if (!tdbi) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &tdbi->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* delayinfo */
	err = jsonr_ref_delay_field(r, "delay", &tdbi->delay);
	if (err < 0) {
		panel_err("failed to parse reference\n");
		goto out_free;
	}

	return 0;

out_free:
	free_pnobj_name(&tdbi->base);
	return err;
}

int jsonr_power_ctrl(json_reader_t *r, struct pwrctrl *pwrctrl)
{
	char buf[SZ_128];
	char str[SZ_128];
	int err;

	if (!pwrctrl) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		goto out_free;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &pwrctrl->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		goto out_free;
	}

	/* key */
	err = jsonr_string_field(r, buf, str);
	if (err < 0) {
		panel_err("failed to parse string\n");
		goto out_free;
	}
	pwrctrl->key = kstrndup(str, PNOBJ_NAME_LEN-1, GFP_KERNEL);

	return 0;

out_free:
	free_pnobj_name(&pwrctrl->base);
	kfree(pwrctrl->key);

	return err;
}

/*
 * parsing enum list
 *
 * example: "enums":{\"VRR_NORMAL_MODE\":0,\"VRR_HS_MODE\":1}
 *
 */
int jsonr_enum_field(json_reader_t *r, const char *key,
		struct panel_property *prop)
{
	char str[SZ_128];
	unsigned int value;
	int i, err;
	jsmntok_t *it_tok = NULL;

	JEXPECT_STREQ(r, current_token(r), key); inc_token(r);
	JEXPECT_OBJECT(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	if (it_tok->size <= 0)
		return -EINVAL;

	for (i = 0; i < it_tok->size / 2; i++) {
		err = jsonr_uint_field(r, str, &value);
		if (err < 0) {
			panel_err("failed to parse uint\n");
			goto out_free;
		}

		err = panel_property_add_enum_value(prop, value, str);
		if (err < 0) {
			panel_err("failed to add enum value(%d:%s)\n", value, str);
			goto out_free;
		}
	}

	return 0;

out_free:
	panel_property_enum_free(prop);
	return err;
}

int jsonr_property(json_reader_t *r, struct panel_property *prop)
{
	char buf[SZ_128];
	char str[SZ_128];
	int err, type;

	if (!prop) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		goto out_free;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &prop->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		goto out_free;
	}

	/* type */
	err = jsonr_string_field(r, buf, str);
	if (err < 0) {
		panel_err("failed to parse string\n");
		goto out_free;
	}

	if (strncmp(buf, "type", SZ_32)) {
		panel_err("wrong key string(%s)\n", buf);
		goto out_free;
	}

	type = string_to_prop_type(str);
	if (type < 0) {
		panel_err("unknown prop type(%s)\n", str);
		goto out_free;
	}
	prop->type = type;

	INIT_LIST_HEAD(&prop->enum_list);
	if (type == PANEL_PROP_TYPE_RANGE) {
		/* min */
		err = jsonr_uint_field(r, buf, &prop->min);
		if (err < 0) {
			panel_err("failed to parse uint\n");
			goto out_free;
		}

		/* max */
		err = jsonr_uint_field(r, buf, &prop->max);
		if (err < 0) {
			panel_err("failed to parse uint\n");
			goto out_free;
		}
	} else if (type == PANEL_PROP_TYPE_ENUM) {
		err = jsonr_enum_field(r, "enums", prop);
		if (err < 0) {
			panel_err("failed to parse enum field\n");
			goto out_free;
		}
	}

	return 0;

out_free:
	free_pnobj_name(&prop->base);

	return err;
}

int jsonr_config(json_reader_t *r, struct pnobj_config *config)
{
	char buf[SZ_128];
	int err;
	struct pnobj *pnobj;

	if (!config) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		goto out_free;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &config->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		goto out_free;
	}

	/* prop */
	err = jsonr_ref_pnobj_field(r, "prop", &pnobj);
	if (err < 0) {
		panel_err("failed to parse property\n");
		goto out_free;
	}
	strncpy(config->prop_name, get_pnobj_name(pnobj), PANEL_PROP_NAME_LEN);
	config->prop_name[PANEL_PROP_NAME_LEN-1] = '\0';

	err = jsonr_uint_field(r, buf, &config->value);
	if (err < 0) {
		panel_err("failed to parse uint\n");
		goto out_free;
	}


	return 0;

out_free:
	free_pnobj_name(&config->base);

	return err;
}

static int jsonr_sequence_prepare(json_reader_t *r, struct seqinfo *seq)
{
	jsmntok_t *it_tok;
	char buf[SZ_128];
	struct pnobj *pnobj = NULL;
	void **cmdtbl = NULL;
	int i, err;

	if (!seq) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}
	seq->size = 0;
	seq->cmdtbl = NULL;

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		return err;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &seq->base);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		return err;
	}

	/* parsing sequence command table */
	JEXPECT_STREQ(r, current_token(r), "cmdtbl"); inc_token(r);
	JEXPECT_ARRAY(r, current_token(r));
	it_tok = current_token(r); inc_token(r);
	if (it_tok->size == 0)
		return 0;

	cmdtbl = kzalloc(sizeof(*cmdtbl) * it_tok->size, GFP_KERNEL);
	for (i = 0; i < it_tok->size; i++) {
		pnobj = kzalloc(sizeof(*pnobj), GFP_KERNEL);
		if (!pnobj) {
			err = -ENOMEM;
			goto out_free;
		}

		err = jsonr_ref_pnobj(r, pnobj);
		if (err < 0) {
			kfree(pnobj);
			goto out_free;
		}

		if (!get_pnobj_name(pnobj)) {
			kfree(pnobj);
			break;
		}

		cmdtbl[i] = pnobj;
		seq->size++;
	}
	seq->cmdtbl = cmdtbl;

	return 0;

out_free:
	for (i = 0; i < seq->size; i++)
		kfree(cmdtbl[i]);
	kfree(cmdtbl);
	free_pnobj_name(&seq->base);

	return err;
}

static int jsonr_sequence_bind(json_reader_t *r, struct seqinfo *seq)
{
	int i, ret = 0;
	struct pnobj *pnobj, *found;

	for (i = 0; i < seq->size; i++) {
		pnobj = seq->cmdtbl[i];
		if (!get_pnobj_name(pnobj)) {
			panel_err("null pnobj in seq(%s)\n",
					get_sequence_name(seq));
			break;
		}

		found = pnobj_find_by_pnobj(get_jsonr_pnobj_list(r), pnobj);
		if (!found) {
			panel_err("pnobj(%s:%s) not found\n",
					get_pnobj_name(pnobj),
					cmd_type_to_string(get_pnobj_cmd_type(pnobj)));
			ret = -EINVAL;
		}
		seq->cmdtbl[i] = found;
		free_pnobj_name(pnobj);
		kfree(pnobj);
	}

	return ret;
}

int jsonr_sequence(json_reader_t *r, struct seqinfo *seq)
{
	int ret;

	ret = jsonr_sequence_prepare(r, seq);
	if (ret < 0) {
		panel_err("failed to parse sequence(%s)\n",
				get_sequence_name(seq));
		return ret;
	}

	ret = jsonr_sequence_bind(r, seq);
	if (ret < 0) {
		panel_err("failed to bind sequence(%s)\n",
				get_sequence_name(seq));
		return ret;
	}

	return 0;
}

int jsonr_function(json_reader_t *r, struct pnobj_func *func)
{
	char buf[SZ_128];
	struct pnobj t_pnobj;
	struct pnobj_func *found;
	int err;

	if (!func) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	/* entry */
	err = jsonr_string(r, buf);
	if (err < 0) {
		panel_err("failed to parse string\n");
		goto out_free;
	}

	/* parsing base */
	err = jsonr_pnobj_object(r, &t_pnobj);
	if (err < 0) {
		panel_err("failed to parse pnobj\n");
		goto out_free;
	}

	found = panel_function_lookup(get_pnobj_name(&t_pnobj));
	if (!found) {
		panel_err("pnobj(%s:%s) not found in panel_function_list\n",
				get_pnobj_name(&t_pnobj),
				cmd_type_to_string(get_pnobj_cmd_type(&t_pnobj)));
		err = -EINVAL;
		goto out_free;
	}

	deepcopy_pnobj_function(func, found);

	return 0;

out_free:
	free_pnobj_name(&func->base);
	return err;
}

struct pnobj *jsonr_element(json_reader_t *r, int pnobj_cmd_type)
{
	int err = 0;
	struct pnobj *pnobj = NULL;

	if (pnobj_cmd_type == CMD_TYPE_MAP) {
		struct maptbl *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_maptbl(r, elem);
	} else if (pnobj_cmd_type == CMD_TYPE_RES) {
		struct resinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_resource(r, elem);
	} else if (pnobj_cmd_type == CMD_TYPE_DMP) {
		struct dumpinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_dumpinfo(r, elem);
	} else if (pnobj_cmd_type == CMD_TYPE_PCTRL) {
		struct pwrctrl *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_power_ctrl(r, elem);
	} else if (pnobj_cmd_type == CMD_TYPE_CFG) {
		struct pnobj_config *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_config(r, elem);
	} else if (IS_CMD_TYPE_SEQ(pnobj_cmd_type)) {
		struct seqinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_sequence_prepare(r, elem);
	} else if (IS_CMD_TYPE_RX_PKT(pnobj_cmd_type)) {
		struct rdinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_rx_packet(r, elem);
	} else if (IS_CMD_TYPE_KEY(pnobj_cmd_type)) {
		struct keyinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_keyinfo(r, elem);
	} else if (IS_CMD_TYPE_TX_PKT(pnobj_cmd_type)) {
		struct pktinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_tx_packet(r, elem);
	} else if (IS_CMD_TYPE_DELAY(pnobj_cmd_type) ||
			(pnobj_cmd_type == CMD_TYPE_TIMER_DELAY)) {
		struct delayinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_delayinfo(r, elem);
	} else if (pnobj_cmd_type == CMD_TYPE_TIMER_DELAY_BEGIN) {
		struct timer_delay_begin_info *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_timer_delay_begin_info(r, elem);
	} else if (IS_CMD_TYPE_COND(pnobj_cmd_type)) {
		struct condinfo *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_condition(r, elem);
	} else if (pnobj_cmd_type == CMD_TYPE_FUNC) {
		struct pnobj_func *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_function(r, elem);
	} else if (IS_CMD_TYPE_PROP(pnobj_cmd_type)) {
		struct panel_property *elem = kzalloc(sizeof(*elem), GFP_KERNEL);

		pnobj = &elem->base;
		err = jsonr_property(r, elem);
	}

	if (err < 0) {
		panel_err("failed to parse %s element\n",
				cmd_type_to_string(pnobj_cmd_type));
		return NULL;
	}

	return pnobj;
}

int jsonr_all(json_reader_t *r)
{
	jsmntok_t *top, *tok, *temp_tok;
	char buf[SZ_128], str[SZ_32+1];
	int i, j, err, pnobj_cmd_type;
	struct pnobj *pos, *next, *pnobj;

	JEXPECT_OBJECT(r, current_token(r));
	top = current_token(r); inc_token(r);
	for (i = 0; i < top->size / 2; i++) {
		err = jsonr_string(r, buf);
		if (err < 0) {
			panel_err("failed to parse string\n");
			goto out_free;
		}

		pnobj_cmd_type = string_to_cmd_type(buf);
		if (pnobj_cmd_type < 0) {
			snprintf(str, SZ_32, "%.*s", SZ_32, buf);
			str[SZ_32] = '\0';
			panel_err("failed to parse type(%s)\n", str);
			err = -EINVAL;
			goto out_free;
		}

		JEXPECT_OBJECT(r, current_token(r));
		tok = current_token(r); inc_token(r);
		for (j = 0; j < tok->size / 2; j++) {
			temp_tok = current_token(r);
			pnobj = jsonr_element(r, pnobj_cmd_type);
			if (!pnobj) {
				snprintf(str, SZ_32, "%.*s", SZ_32, get_jsonr_buf(r) + temp_tok->start);
				panel_err("failed to parse %s:%s\n",
						cmd_type_to_string(pnobj_cmd_type), str);
				err = -EINVAL;
				goto out_free;
			}

			if (pnobj_cmd_type == CMD_TYPE_FUNC &&
					pnobj_find_by_name(get_jsonr_pnobj_list(r),
						get_pnobj_name(pnobj))) {
				panel_dbg("function(%s) already exist\n",
						get_pnobj_name(pnobj));
				continue;
			}

			list_add_tail(get_pnobj_list(pnobj), get_jsonr_pnobj_list(r));

			panel_dbg("%.*s\n", SZ_32, get_jsonr_buf(r) + temp_tok->start);
		}
	}

	/* replace allocated pnobj to pnobj pointer */
	list_for_each_entry(pos, get_jsonr_pnobj_list(r), list) {
		struct seqinfo *seq;

		if (!IS_CMD_TYPE_SEQ(get_pnobj_cmd_type(pos)))
			continue;

		seq = pnobj_container_of(pos, struct seqinfo);
		err = jsonr_sequence_bind(r, seq);
		if (err < 0) {
			panel_err("failed to bind sequence(%s)\n",
					get_sequence_name(seq));
			goto out_free;
		}
	}

	panel_info("setup pnobj_list successfully\n");

	return 0;

out_free:
	list_for_each_entry_safe(pos, next, get_jsonr_pnobj_list(r), list)
		destroy_panel_object(pos);

	return err;
}

int panel_json_parse(char *json, size_t size, struct list_head *pnobj_list)
{
	json_reader_t *r;
	jsmnerr_t err;
	int ret;

	r = jsonr_new(size, pnobj_list);
	if (!r) {
		panel_err("failed to create json reader stream\n");
		return -EINVAL;
	}

	err = jsonr_parse(r, json);
	if (err != JSMN_SUCCESS) {
		panel_err("failed to parse json\n");
		ret = -EINVAL;
		goto err;
	}

	ret = jsonr_all(r);
	if (ret < 0) {
		panel_err("failed to parse all json pnobj\n");
		goto err;
	}

	jsonr_destroy(&r);

	return 0;

err:
	jsonr_destroy(&r);
	return ret;
}


__visible_for_testing void jsonw_pnobj(json_writer_t *w, struct pnobj *pnobj)
{
	jsonw_name(w, "pnobj");
	jsonw_start_object(w);
	{
		jsonw_string_field(w, "type",
				cmd_type_to_string(get_pnobj_cmd_type(pnobj)));
		jsonw_string_field(w, "name", pnobj->name);
	}
	jsonw_end_object(w);
}

static void jsonw_ref(json_writer_t *w,
		const char *path, const char *key)
{
	char refname[PNOBJ_FUNC_NAME_LEN + 32];

	jsonw_start_object(w);
	{
		snprintf(refname, sizeof(refname), "#/%s/%s", path, key);
		jsonw_string_field(w, "$ref", refname);
	}
	jsonw_end_object(w);
}

static void jsonw_ref_field(json_writer_t *w,
		const char *prop, const char *path, const char *key)
{
	jsonw_name(w, prop);
	jsonw_ref(w, path, key);
}

static void jsonw_ref_function_field(json_writer_t *w,
		const char *prop, struct pnobj_func *f)
{
	char *name;

	if (!f) {
		panel_err("function is null\n");
		jsonw_null_object_field(w, prop);
		return;
	}

	name = get_pnobj_function_name(f);
	if (!name) {
		panel_err("failed to get function name\n");
		jsonw_null_object_field(w, prop);
		return;
	}

	jsonw_ref_field(w, prop, "FUNCTION", name);
}

static void jsonw_ref_pnobj_field(json_writer_t *w,
		const char *prop, struct pnobj *pnobj)
{
	if (!pnobj) {
		jsonw_null_object_field(w, prop);
		return;
	}

	jsonw_ref_field(w, prop, pnobj_cmd_type_to_json_path(
				get_pnobj_cmd_type(pnobj)), pnobj->name);
}

bool need_rle(u8 *arr, size_t size)
{
	int i;

	if (size < 128)
		return false;

	for (i = 1; i < size; i++) {
		if (arr[0] != arr[i])
			return false;
	}

	return true;
}

__visible_for_testing void jsonw_u8_rle(json_writer_t *w,
		u8 *arr, size_t size)
{
	int i, j = 0, rlen;
	unsigned int *rle_data = kvmalloc(size, GFP_KERNEL);
	unsigned int *rle_len = kvmalloc(size, GFP_KERNEL);

	for (i = 0; i < size; i++) {
		rle_data[j] = arr[i];

		rlen = 1;
		while (i + 1 < size && arr[i] == arr[i + 1]) {
			rlen++;
			i++;
		}

		rle_len[j++] = rlen;
	}

	/* unique data */
	jsonw_start_array(w);
	for (i = 0; i < j; i++)
		jsonw_int(w, rle_data[i]);
	jsonw_end_array(w);
	kvfree(rle_data);

	/* length of data */
	jsonw_start_array(w);
	for (i = 0; i < j; i++)
		jsonw_int(w, rle_len[i]);
	jsonw_end_array(w);
	kvfree(rle_len);
}

__visible_for_testing void jsonw_byte_array_field(json_writer_t *w,
		const char *prop, u8 *arr, size_t size)
{
	size_t i;

	jsonw_name(w, prop);
	jsonw_start_array(w);
	if (need_rle(arr, size)) {
		jsonw_u8_rle(w, arr, size);
	} else {
		for (i = 0; i < size; i++)
			jsonw_int(w, arr ? arr[i] : 0);
	}
	jsonw_end_array(w);
}

static void jsonw_maptbl_shape(json_writer_t *w, struct maptbl *m)
{
	size_t i;

	jsonw_name(w, "shape");
	jsonw_start_object(w);
	{
		jsonw_int_field(w, "nr_dimen", maptbl_get_countof_dimen(m));
		jsonw_name(w, "sz_dimen");
		jsonw_start_array(w);
		maptbl_for_each_dimen(m, i)
			jsonw_int(w, maptbl_get_countof_n_dimen_element(m, i));
		jsonw_end_array(w);
	}
	jsonw_end_object(w);
}

static void jsonw_maptbl_array(json_writer_t *w, struct maptbl *m)
{
	jsonw_byte_array_field(w, "arr",
			m->arr, maptbl_get_sizeof_maptbl(m));
}

static void jsonw_maptbl_ops(json_writer_t *w, struct maptbl *m)
{
	jsonw_name(w, "ops");
	jsonw_start_object(w);
	{
		jsonw_ref_function_field(w, "init", m->ops.init);
		jsonw_ref_function_field(w, "getidx", m->ops.getidx);
		jsonw_ref_function_field(w, "copy", m->ops.copy);
	}
	jsonw_end_object(w);
}

static void jsonw_maptbl_props(json_writer_t *w, struct maptbl *m)
{
	size_t i;

	jsonw_name(w, "props");
	jsonw_start_array(w);
	maptbl_for_each_dimen(m, i) {
		if (!m->props.name[i])
			continue;
		jsonw_ref(w, "PROPERTY", m->props.name[i]);
	}
	jsonw_end_array(w);
}

int jsonw_maptbl(json_writer_t *w, struct maptbl *m)
{
	jsonw_name(w, maptbl_get_name(m));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)m);
		jsonw_maptbl_shape(w, m);
		jsonw_maptbl_array(w, m);
		jsonw_maptbl_ops(w, m);
		jsonw_maptbl_props(w, m);
	}
	jsonw_end_object(w);

	return 0;
}

static int jsonw_packet_update_info(json_writer_t *w, struct pkt_update_info *pktui)
{
	jsonw_start_object(w);
	jsonw_uint_field(w, "offset", pktui->offset);
	jsonw_ref_pnobj_field(w, "maptbl", (struct pnobj *)pktui->maptbl);
	/* TODO: remove it when EasyOpcode Editor is fixed */
	jsonw_ref_function_field(w, "getidx", NULL);
	jsonw_end_object(w);

	return 0;
}

static int jsonw_packet_update_info_array(json_writer_t *w, struct pkt_update_info *pktui, u32 size)
{
	size_t i;

	jsonw_name(w, "pktui");
	jsonw_start_array(w);
	for (i = 0; i < size; i++)
		jsonw_packet_update_info(w, pktui + i);
	jsonw_end_array(w);

	return 0;
}

int jsonw_tx_packet(json_writer_t *w, struct pktinfo *pkt)
{
	jsonw_name(w, get_pktinfo_name(pkt));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)pkt);
		jsonw_string_field(w, "type",
				packet_type_to_string(get_pktinfo_type(pkt)));
		jsonw_byte_array_field(w, "data",
				pkt->initdata, pkt->dlen);
		jsonw_uint_field(w, "offset", pkt->offset);
		jsonw_packet_update_info_array(w, pkt->pktui, pkt->nr_pktui);
		jsonw_uint_field(w, "option", pkt->option);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_keyinfo(json_writer_t *w, struct keyinfo *key)
{
	jsonw_name(w, get_key_name(key));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)key);
		jsonw_int_field(w, "level", key->level);
		jsonw_int_field(w, "en", key->en);
		jsonw_ref_pnobj_field(w, "packet", (struct pnobj *)key->packet);
	}
	jsonw_end_object(w);

	return 0;
}

static int jsonw_panel_expr_data(json_writer_t *w, struct panel_expr_data *data)
{
	jsonw_start_object(w);
	{
		jsonw_string_field(w, "type",
				panel_expr_type_enum_to_string(data->type));
		if (data->type == PANEL_EXPR_TYPE_OPERAND_U32)
			jsonw_uint_field(w, "op", data->op.u32);
		else if (data->type == PANEL_EXPR_TYPE_OPERAND_S32)
			jsonw_int_field(w, "op", data->op.s32);
		else if (data->type == PANEL_EXPR_TYPE_OPERAND_STR)
			jsonw_string_field(w, "op", data->op.str);
		else if (data->type == PANEL_EXPR_TYPE_OPERAND_PROP)
			jsonw_string_field(w, "op", data->op.str);
		else if (data->type == PANEL_EXPR_TYPE_OPERAND_FUNC)
			jsonw_ref_function_field(w, "op", (void *)data->op.func);
	}
	jsonw_end_object(w);

	return 0;
}

static int jsonw_condition_rule(json_writer_t *w, struct cond_rule *rule)
{
	size_t i;

	jsonw_name(w, "rule");
	jsonw_start_object(w);
	{
		jsonw_name(w, "item");
		jsonw_start_array(w);
		for (i = 0; i < rule->num_item; i++)
			jsonw_panel_expr_data(w, &rule->item[i]);
		jsonw_end_array(w);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_condition(json_writer_t *w, struct condinfo *c)
{
	jsonw_name(w, get_condition_name(c));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)c);
		jsonw_condition_rule(w, &c->rule);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_rx_packet(json_writer_t *w, struct rdinfo *rdi)
{
	jsonw_name(w, get_rdinfo_name(rdi));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)rdi);
		jsonw_string_field(w, "type",
				packet_type_to_string(get_rdinfo_type(rdi)));
		jsonw_uint_field(w, "addr", rdi->addr);
		jsonw_uint_field(w, "offset", rdi->offset);
		jsonw_uint_field(w, "len", rdi->len);
	}
	jsonw_end_object(w);

	return 0;
}

static int jsonw_resource_update_info(json_writer_t *w, struct res_update_info *resui)
{
	jsonw_start_object(w);
	jsonw_uint_field(w, "offset", resui->offset);
	jsonw_ref_pnobj_field(w, "rdi", (struct pnobj *)resui->rditbl);
	jsonw_end_object(w);

	return 0;
}

static int jsonw_resource_update_info_array(json_writer_t *w, struct res_update_info *resui, u32 size)
{
	size_t i;

	jsonw_name(w, "resui");
	jsonw_start_array(w);
	for (i = 0; i < size; i++)
		jsonw_resource_update_info(w, resui + i);
	jsonw_end_array(w);

	return 0;
}

int jsonw_resource(json_writer_t *w, struct resinfo *res)
{
	/* fill 0 if mutable resource */
	u8 *arr = is_resource_mutable(res) ? NULL : res->data;

	jsonw_name(w, get_resource_name(res));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)res);
		jsonw_byte_array_field(w, "data", arr, res->dlen);
		jsonw_resource_update_info_array(w, res->resui, res->nr_resui);
	}
	jsonw_end_object(w);

	return 0;
}

static int jsonw_dump_expect(json_writer_t *w, struct dump_expect *expect)
{
	jsonw_start_object(w);
	jsonw_uint_field(w, "offset", expect->offset);
	jsonw_uint_field(w, "mask", expect->mask);
	jsonw_uint_field(w, "value", expect->value);
	jsonw_string_field(w, "msg", expect->msg);
	jsonw_end_object(w);

	return 0;
}

int jsonw_dumpinfo(json_writer_t *w, struct dumpinfo *dump)
{
	int i;

	jsonw_name(w, get_dump_name(dump));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)dump);
		jsonw_ref_pnobj_field(w, "res", (struct pnobj *)dump->res);
		jsonw_ref_function_field(w, "callback", (void *)dump->ops.show);
		jsonw_name(w, "expects");
		jsonw_start_array(w);
		for (i = 0; i < dump->nr_expects; i++)
			jsonw_dump_expect(w, &dump->expects[i]);
		jsonw_end_array(w);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_delayinfo(json_writer_t *w, struct delayinfo *di)
{
	jsonw_name(w, get_delay_name(di));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)di);
		jsonw_uint_field(w, "usec", di->usec);
		jsonw_uint_field(w, "nframe", di->nframe);
		jsonw_uint_field(w, "nvsync", di->nvsync);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_timer_delay_begin_info(json_writer_t *w, struct timer_delay_begin_info *tdbi)
{
	jsonw_name(w, get_timer_delay_begin_name(tdbi));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)tdbi);
		jsonw_ref_pnobj_field(w, "delay", (struct pnobj *)tdbi->delay);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_power_ctrl(json_writer_t *w, struct pwrctrl *pi)
{
	jsonw_name(w, get_pwrctrl_name(pi));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)pi);
		jsonw_string_field(w, "key", get_pwrctrl_key(pi));
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_enum_field(json_writer_t *w, char *key, struct panel_property *prop)
{
	struct panel_property_enum *prop_enum;

	jsonw_name(w, key);
	jsonw_start_object(w);
	list_for_each_entry(prop_enum, &prop->enum_list, head) {
		jsonw_uint_field(w, prop_enum->name, prop_enum->value);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_property(json_writer_t *w, struct panel_property *prop)
{
	jsonw_name(w, get_panel_property_name(prop));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)&prop->base);
		jsonw_string_field(w, "type", prop_type_to_string(prop->type));

		if (panel_property_type_is(prop, PANEL_PROP_TYPE_RANGE)) {
			jsonw_uint_field(w, "min", prop->min);
			jsonw_uint_field(w, "max", prop->max);
		} else if (panel_property_type_is(prop, PANEL_PROP_TYPE_ENUM)) {
			jsonw_enum_field(w, "enums", prop);
		}
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_config(json_writer_t *w, struct pnobj_config *config)
{
	jsonw_name(w, get_pnobj_config_name(config));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)config);
		jsonw_ref_field(w, "prop", "PROPERTY", config->prop_name);
		jsonw_uint_field(w, "value", config->value);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_sequence(json_writer_t *w, struct seqinfo *seq)
{
	size_t i;

	jsonw_name(w, get_sequence_name(seq));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)seq);
		jsonw_name(w, "cmdtbl");
		jsonw_start_array(w);
		for (i = 0; i < seq->size; i++) {
			struct pnobj *elem =
				(struct pnobj *)seq->cmdtbl[i];

			if (elem)
				jsonw_ref(w, pnobj_cmd_type_to_json_path(
							get_pnobj_cmd_type(elem)), elem->name);
			else
				jsonw_null(w);
		}
		jsonw_end_array(w);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_function(json_writer_t *w, struct pnobj_func *func)
{
	jsonw_name(w, get_pnobj_name(&func->base));
	jsonw_start_object(w);
	{
		jsonw_pnobj(w, (struct pnobj *)func);
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_element(json_writer_t *w, struct pnobj *pnobj)
{
	u32 pnobj_cmd_type;

	if (!pnobj)
		return -EINVAL;

	pnobj_cmd_type = get_pnobj_cmd_type(pnobj);

	if (pnobj_cmd_type == CMD_TYPE_MAP)
		jsonw_maptbl(w, (struct maptbl *)pnobj);
	else if (pnobj_cmd_type == CMD_TYPE_RES)
		jsonw_resource(w, (struct resinfo *)pnobj);
	else if (pnobj_cmd_type == CMD_TYPE_DMP)
		jsonw_dumpinfo(w, (struct dumpinfo *)pnobj);
	else if (pnobj_cmd_type == CMD_TYPE_PCTRL)
		jsonw_power_ctrl(w, (struct pwrctrl *)pnobj);
	else if (pnobj_cmd_type == CMD_TYPE_CFG)
		jsonw_config(w, (struct pnobj_config *)pnobj);
	else if (pnobj_cmd_type == CMD_TYPE_SEQ)
		jsonw_sequence(w, (struct seqinfo *)pnobj);
	else if (IS_CMD_TYPE_RX_PKT(pnobj_cmd_type))
		jsonw_rx_packet(w, (struct rdinfo *)pnobj);
	else if (IS_CMD_TYPE_KEY(pnobj_cmd_type))
		jsonw_keyinfo(w, (struct keyinfo *)pnobj);
	else if (IS_CMD_TYPE_TX_PKT(pnobj_cmd_type))
		jsonw_tx_packet(w, (struct pktinfo *)pnobj);
	else if (IS_CMD_TYPE_DELAY(pnobj_cmd_type) ||
			(pnobj_cmd_type == CMD_TYPE_TIMER_DELAY))
		jsonw_delayinfo(w, (struct delayinfo *)pnobj);
	else if (pnobj_cmd_type == CMD_TYPE_TIMER_DELAY_BEGIN)
		jsonw_timer_delay_begin_info(w, (struct timer_delay_begin_info *)pnobj);
	else if (IS_CMD_TYPE_COND(pnobj_cmd_type))
		jsonw_condition(w, (struct condinfo *)pnobj);
	else if (pnobj_cmd_type == CMD_TYPE_FUNC)
		jsonw_function(w, (struct pnobj_func *)pnobj);
	else if (IS_CMD_TYPE_PROP(pnobj_cmd_type))
		jsonw_property(w, (struct panel_property *)pnobj);
	else {
		panel_warn("invalid panel object type(%d)\n", pnobj_cmd_type);
		return -EINVAL;
	}

	return 0;
}

int jsonw_pnobj_list(json_writer_t *w, u32 pnobj_cmd_type, struct list_head *pnobj_list)
{
	struct pnobj *pnobj;

	jsonw_name(w, pnobj_cmd_type_to_json_path(pnobj_cmd_type));
	jsonw_start_object(w);
	{
		list_for_each_entry(pnobj, pnobj_list, list) {
			if (pnobj_cmd_type != get_pnobj_cmd_type(pnobj))
				continue;

			jsonw_element(w, pnobj);
		}
	}
	jsonw_end_object(w);

	return 0;
}

int jsonw_sorted_pnobj_list(json_writer_t *w, u32 pnobj_cmd_type, struct list_head *pnobj_list)
{
	struct pnobj_refs *refs;
	struct pnobj_ref *ref;
	struct pnobj *pnobj;

	/* make sorted pnobj_refs list */
	refs = pnobj_list_to_pnobj_refs(pnobj_list);
	if (!refs)
		return -EINVAL;

	list_sort(NULL, &refs->list, pnobj_ref_compare);

	jsonw_name(w, pnobj_cmd_type_to_json_path(pnobj_cmd_type));
	jsonw_start_object(w);
	{
		list_for_each_entry(ref, &refs->list, list) {
			pnobj = ref->pnobj;
			if (pnobj_cmd_type != get_pnobj_cmd_type(pnobj))
				continue;

			jsonw_element(w, pnobj);
		}
	}
	jsonw_end_object(w);
	remove_pnobj_refs(refs);

	return 0;
}

int jsonw_maptbl_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_MAP, head);
}

int jsonw_rx_packet_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_RX_PACKET, head);
}

int jsonw_resource_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_RES, head);
}

int jsonw_dumpinfo_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_DMP, head);
}

int jsonw_power_ctrl_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_PCTRL, head);
}

int jsonw_property_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_PROP, head);
}

int jsonw_config_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_CFG, head);
}

int jsonw_sequence_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_SEQ, head);
}

int jsonw_key_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_KEY, head);
}

int jsonw_tx_packet_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_TX_PACKET, head);
}

int jsonw_delay_list(json_writer_t *w, struct list_head *head)
{
	jsonw_pnobj_list(w, CMD_TYPE_DELAY, head);
	jsonw_pnobj_list(w, CMD_TYPE_TIMER_DELAY, head);
	jsonw_pnobj_list(w, CMD_TYPE_TIMER_DELAY_BEGIN, head);

	return 0;
}

int jsonw_condition_list(json_writer_t *w, struct list_head *head)
{
	jsonw_pnobj_list(w, CMD_TYPE_COND_IF, head);
	jsonw_pnobj_list(w, CMD_TYPE_COND_EL, head);
	jsonw_pnobj_list(w, CMD_TYPE_COND_FI, head);

	return 0;
}

int jsonw_function_list(json_writer_t *w, struct list_head *head)
{
	return jsonw_pnobj_list(w, CMD_TYPE_FUNC, head);
}

MODULE_DESCRIPTION("panel object json read/write driver");
MODULE_LICENSE("GPL");
