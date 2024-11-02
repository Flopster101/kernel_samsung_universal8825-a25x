// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Simple streaming JSON writer
 *
 * This takes care of the annoying bits of JSON syntax like the commas
 * after elements
 *
 * Authors:	Stephen Hemminger <stephen@networkplumber.org>
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "json_writer.h"

struct json_writer {
	char *buf; /* output buffer */
	size_t pos; /* current buffer position */
	size_t size;	/* output buffer total size */
	unsigned depth;  /* nesting */
	bool pretty; /* optional whitepace */
	char sep;	/* either nul or comma */
};

static int jsonw_putc(json_writer_t *self, char c)
{
	self->pos += snprintf(self->buf + self->pos, self->size - self->pos, "%c", c);
	return self->pos;
}

static int jsonw_puts(json_writer_t *self, char *s)
{
	self->pos += snprintf(self->buf + self->pos, self->size - self->pos, "%s", s);
	return self->pos;
}

/* indentation for pretty print */
static void jsonw_indent(json_writer_t *self)
{
	unsigned i;
	for (i = 0; i < self->depth; ++i)
		jsonw_puts(self, "    ");
}

/* end current line and indent if pretty printing */
static void jsonw_eol(json_writer_t *self)
{
	if (!self->pretty)
		return;

	jsonw_putc(self, '\n');
	jsonw_indent(self);
}

/* If current object is not empty print a comma */
static void jsonw_eor(json_writer_t *self)
{
	if (self->sep != '\0')
		jsonw_putc(self, self->sep);
	self->sep = ',';
}

/* Output JSON encoded string */
/* Handles C escapes, does not do Unicode */
static void jsonw_put_string(json_writer_t *self, const char *str)
{
	jsonw_putc(self, '"');
	for (; *str; ++str)
		switch (*str) {
		case '\t':
			jsonw_puts(self, "\\t");
			break;
		case '\n':
			jsonw_puts(self, "\\n");
			break;
		case '\r':
			jsonw_puts(self, "\\r");
			break;
		case '\f':
			jsonw_puts(self, "\\f");
			break;
		case '\b':
			jsonw_puts(self, "\\b");
			break;
		case '\\':
			jsonw_puts(self, "\\n");
			break;
		case '"':
			jsonw_puts(self, "\\\"");
			break;
		case '\'':
			jsonw_puts(self, "\\\'");
			break;
		default:
			jsonw_putc(self, *str);
		}
	jsonw_putc(self, '"');
}

/* Create a new JSON stream */
json_writer_t *jsonw_new(char *buf, unsigned long size)
{
	json_writer_t *self;
	
	self = kzalloc(sizeof(*self), GFP_KERNEL);
	if (!self)
		return NULL;

	self->buf = buf;
	self->pos = 0;
	self->size = size;
	self->depth = 0;
	self->pretty = false;
	self->sep = '\0';

	return self;
}

/* End output to JSON stream */
void jsonw_destroy(json_writer_t **self_p)
{
	json_writer_t *self = *self_p;

	BUG_ON(self->depth != 0);
	jsonw_puts(self, "\n");
	kfree(self);
	*self_p = NULL;
}

void jsonw_pretty(json_writer_t *self, bool on)
{
	self->pretty = on;
}

void jsonw_reset(json_writer_t *self)
{
	BUG_ON(self->depth != 0);
	self->sep = '\0';
}

/* Basic blocks */
static void jsonw_begin(json_writer_t *self, int c)
{
	jsonw_eor(self);
	jsonw_putc(self, c);
	++self->depth;
	self->sep = '\0';
}

static void jsonw_end(json_writer_t *self, int c)
{
	BUG_ON(self->depth <= 0);

	--self->depth;
	if (self->sep != '\0')
		jsonw_eol(self);
	jsonw_putc(self, c);
	self->sep = ',';
}


/* Add a JSON property name */
void jsonw_name(json_writer_t *self, const char *name)
{
	jsonw_eor(self);
	jsonw_eol(self);
	self->sep = '\0';
	jsonw_put_string(self, name ? name : "");
	jsonw_putc(self, ':');
	if (self->pretty)
		jsonw_putc(self, ' ');
}

void jsonw_vprintf_enquote(json_writer_t *self, const char *fmt, va_list ap)
{
	jsonw_eor(self);
	jsonw_putc(self, '"');
	self->pos += vsnprintf(self->buf + self->pos, self->size - self->pos, fmt, ap);
	jsonw_putc(self, '"');
}

void jsonw_printf(json_writer_t *self, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	jsonw_eor(self);
	self->pos += vsnprintf(self->buf + self->pos, self->size - self->pos, fmt, ap);
	va_end(ap);
}

/* Collections */
void jsonw_start_object(json_writer_t *self)
{
	jsonw_begin(self, '{');
}

void jsonw_end_object(json_writer_t *self)
{
	jsonw_end(self, '}');
}

void jsonw_start_array(json_writer_t *self)
{
	jsonw_begin(self, '[');
}

void jsonw_end_array(json_writer_t *self)
{
	self->sep = '\0';
	jsonw_end(self, ']');
}

/* JSON value types */
void jsonw_string(json_writer_t *self, const char *value)
{
	jsonw_eor(self);
	jsonw_put_string(self, value ? value : "");
}

void jsonw_bool(json_writer_t *self, bool val)
{
	jsonw_printf(self, "%s", val ? "true" : "false");
}

void jsonw_null(json_writer_t *self)
{
	jsonw_printf(self, "{}");
}

void jsonw_ushort(json_writer_t *self, unsigned short num)
{
	jsonw_printf(self, "%hu", num);
}

void jsonw_uint(json_writer_t *self, uint64_t num)
{
	jsonw_printf(self, "%llu", num);
}

void jsonw_lluint(json_writer_t *self, unsigned long long int num)
{
	jsonw_printf(self, "%llu", num);
}

void jsonw_int(json_writer_t *self, int64_t num)
{
	jsonw_printf(self, "%lld", num);
}

/* Basic name/value objects */
void jsonw_string_field(json_writer_t *self, const char *prop, const char *val)
{
	jsonw_name(self, prop);
	jsonw_string(self, val);
}

void jsonw_bool_field(json_writer_t *self, const char *prop, bool val)
{
	jsonw_name(self, prop);
	jsonw_bool(self, val);
}

void jsonw_uint_field(json_writer_t *self, const char *prop, uint64_t num)
{
	jsonw_name(self, prop);
	jsonw_uint(self, num);
}

void jsonw_ushort_field(json_writer_t *self, const char *prop, unsigned short num)
{
	jsonw_name(self, prop);
	jsonw_ushort(self, num);
}

void jsonw_lluint_field(json_writer_t *self,
			const char *prop,
			unsigned long long int num)
{
	jsonw_name(self, prop);
	jsonw_lluint(self, num);
}

void jsonw_int_field(json_writer_t *self, const char *prop, int64_t num)
{
	jsonw_name(self, prop);
	jsonw_int(self, num);
}

void jsonw_null_object_field(json_writer_t *self, const char *prop)
{
	jsonw_name(self, prop);
	jsonw_null(self);
}

void jsonw_null_string_field(json_writer_t *self, const char *prop)
{
	jsonw_string_field(self, prop, "");
}
