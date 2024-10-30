// SPDX-License-Identifier: GPL-2.0
/*
 * Simple streaming JSON reader 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <linux/mm.h>
#include "jsmn.h"
#include "json.h"
#include "json_reader.h"

struct json_reader {
	char *buf; /* input buffer */
	jsmntok_t *tokens;
	size_t pos; /* current token position */
	size_t size;	/* token total size */
	struct list_head *pnobj_list;
};

int jsonr_parse(json_reader_t *r, char *buf)
{
	jsmn_parser parser;

	jsmn_init(&parser);
	r->buf = buf;	
	r->pos = 0;

	return jsmn_parse(&parser, r->buf, strnlen(r->buf, r->size), r->tokens, r->size);
}

char *get_jsonr_buf(json_reader_t *r)
{
	return r->buf;
}

struct list_head *get_jsonr_pnobj_list(json_reader_t *r)
{
	return r->pnobj_list;
}

jsmntok_t *current_token(json_reader_t *r)
{
	return r->tokens + r->pos;
}

jsmntype_t current_token_type(json_reader_t *r)
{
	return current_token(r)->type;
}

void inc_token(json_reader_t *r)
{
	r->pos++;
}

int jsonr_uint(json_reader_t *r, unsigned int *value)
{
	char buf[SZ_128];
	int err;

	JEXPECT_PRIMITIVE(r, current_token(r));
	json_strcpy(get_jsonr_buf(r), current_token(r), buf);
	err = kstrtouint(buf, 0, value);
	if (err < 0)
		return -EINVAL;
	inc_token(r);

	return 0;

out_free:
	return err;
}

int jsonr_u8(json_reader_t *r, u8 *value)
{
	char buf[SZ_128];
	int err;

	JEXPECT_PRIMITIVE(r, current_token(r));
	json_strcpy(get_jsonr_buf(r), current_token(r), buf);
	err = kstrtou8(buf, 0, value);
	if (err < 0)
		return -EINVAL;
	inc_token(r);

	return 0;

out_free:
	return err;
}

int jsonr_uint_array(json_reader_t *r,
		unsigned int *arr, unsigned int size)
{
	int i, err;

	for (i = 0; i < size; i++) {
		err = jsonr_uint(r, &arr[i]);
		if (err < 0)
			return err;
	}

	return 0;
}

int jsonr_u8_array(json_reader_t *r,
		unsigned char *arr, unsigned int size)
{
	int i, err;

	for (i = 0; i < size; i++) {
		err = jsonr_u8(r, &arr[i]);
		if (err < 0)
			return err;
	}

	return 0;
}

int jsonr_string(json_reader_t *r, char *s)
{
	int err;

	JEXPECT_STRING(r, current_token(r));
	json_strcpy(get_jsonr_buf(r), current_token(r), s);
	inc_token(r);

	return 0;

out_free:
	return err;
}

int jsonr_uint_field(json_reader_t *r,
		char *key, unsigned int *value)
{
	int err;

	err = jsonr_string(r, key);
	if (err < 0)
		return err;

	err = jsonr_uint(r, value);
	if (err < 0)
		return err;

	return 0;
}

int jsonr_u8_field(json_reader_t *r,
		char *key, u8 *value)
{
	int err;

	err = jsonr_string(r, key);
	if (err < 0)
		return err;

	err = jsonr_u8(r, value);
	if (err < 0)
		return err;

	return 0;
}

int jsonr_string_field(json_reader_t *r,
		char *key, char *value)
{
	int err;

	err = jsonr_string(r, key);
	if (err < 0)
		return err;

	err = jsonr_string(r, value);
	if (err < 0)
		return err;

	return 0;
}

/* Create a new JSON stream */
json_reader_t *jsonr_new(size_t size, struct list_head *pnobj_list)
{
	json_reader_t *self;
	
	self = kzalloc(sizeof(*self), GFP_KERNEL);
	if (!self)
		return NULL;

	self->tokens = kvmalloc(sizeof(*self->tokens) * size, GFP_KERNEL);
	if (!self->tokens) {
		panel_err("failed to alloc json buffer\n");
		kfree(self);
		return NULL;
	}

	self->pos = 0;
	self->size = size;
	self->pnobj_list = pnobj_list;

	return self;
}

/* End output to JSON stream */
void jsonr_destroy(json_reader_t **self_p)
{
	json_reader_t *self = *self_p;

	kvfree(self->tokens);
	kfree(self);
	*self_p = NULL;
}
