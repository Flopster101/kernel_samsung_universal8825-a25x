/* SPDX-License-Identifier: GPL-2.0 */
#ifndef JSON_H
#define JSON_H 1

#include "jsmn.h"

jsmntok_t *parse_json(char *map, size_t size, int *len);
void free_json(char *map, size_t size, jsmntok_t *tokens);
int json_line(char *map, jsmntok_t *t);
const char *json_name(jsmntok_t *t);
int json_streq(char *map, jsmntok_t *t, const char *s);
int json_len(jsmntok_t *t);
void json_strcpy(char *map, jsmntok_t *t, char *dst);

extern int verbose;

#ifndef roundup
#define roundup(x, y) (                                \
{                                                      \
        const typeof(y) __y = y;                       \
        (((x) + (__y - 1)) / __y) * __y;               \
}                                                      \
)
#endif

#endif
