// SPDX-License-Identifier: GPL-2.0
/*
 * Simple streaming JSON reader 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _JSON_READER_H_
#define _JSON_READER_H_

#include <linux/string.h>
#include "panel_debug.h"
#include "jsmn.h"

typedef struct json_reader json_reader_t;

#define JEXPECT(_map, _t, _e, _msg, ...) \
	do { \
		if (!(_e)) { \
			jsmntok_t *loc = (_t); \
			if (!(_t)->start) \
			loc = (_t) - 1; \
			pr_err("%s:%d: %d:%.*s: " _msg, \
					__func__, __LINE__, \
					json_line((_map), loc), json_len(_t), \
					(_map) + (_t)->start, ##__VA_ARGS__); \
			err = -EIO; \
			goto out_free; \
		} \
	} while (0)

#define JSMN_IS_PRIMITIVE(_t) ((_t)->type == JSMN_PRIMITIVE)
#define JSMN_IS_OBJECT(_t) ((_t)->type == JSMN_OBJECT)
#define JSMN_IS_ARRAY(_t) ((_t)->type == JSMN_ARRAY)
#define JSMN_IS_STRING(_t) ((_t)->type == JSMN_STRING)

#define JEXPECT_PRIMITIVE(_r, _t) \
	JEXPECT(get_jsonr_buf(_r), (_t), JSMN_IS_PRIMITIVE(_t), "expected primitive, but got %s", json_name(_t))

#define JEXPECT_OBJECT(_r, _t) \
	JEXPECT(get_jsonr_buf(_r), (_t), JSMN_IS_OBJECT(_t), "expected object, but got %s", json_name(_t))

#define JEXPECT_ARRAY(_r, _t) \
	JEXPECT(get_jsonr_buf(_r), (_t), JSMN_IS_ARRAY(_t), "expected array, but got %s", json_name(_t))

#define JEXPECT_STRING(_r, _t) \
	JEXPECT(get_jsonr_buf(_r), (_t), JSMN_IS_STRING(_t), "expected string, but got %s", json_name(_t))

#define JEXPECT_STREQ(_r, _t, _str) \
	JEXPECT_STRING((_r), _t); \
	JEXPECT(get_jsonr_buf(_r), (_t), json_streq(get_jsonr_buf(_r), (_t), (_str)), "expected %s, but got %s", (_str), get_jsonr_buf(_r) + (_t)->start);

int jsonr_parse(json_reader_t *r, char *buf);
char *get_jsonr_buf(json_reader_t *r);
struct list_head *get_jsonr_pnobj_list(json_reader_t *r);
jsmntok_t *current_token(json_reader_t *r);
jsmntype_t current_token_type(json_reader_t *r);
void inc_token(json_reader_t *r);

int jsonr_uint(json_reader_t *r, unsigned int *value);
int jsonr_u8(json_reader_t *r, u8 *value);
int jsonr_string(json_reader_t *r, char *s);

int jsonr_uint_field(json_reader_t *r,
		char *key, unsigned int *value);
int jsonr_u8_field(json_reader_t *r,
		char *key, u8 *value);
int jsonr_string_field(json_reader_t *r,
		char *key, char *value);

int jsonr_uint_array(json_reader_t *r,
		unsigned int *arr, unsigned int size);
int jsonr_u8_array(json_reader_t *r,
		unsigned char *arr, unsigned int size);

json_reader_t *jsonr_new(size_t size, struct list_head *pnobj_list);
void jsonr_destroy(json_reader_t **self_p);

#endif /* _JSON_WRITER_H_ */
