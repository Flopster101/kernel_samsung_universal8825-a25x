/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/kernel.h>
#include <scsc/scsc_logring.h>
#include "fhdr.h"

/** Return the next item after a given item and decrement the remaining length */
static const struct fhdr_tag_length *fhdr_next_item(const struct fhdr_tag_length *item, uint32_t *fhdr_length)
{
	uint32_t skip_length = sizeof(*item) + item->length;

	if (skip_length < *fhdr_length) {
		*fhdr_length -= skip_length;
		return (const struct fhdr_tag_length *)((uintptr_t)item + skip_length);
	}

	return NULL;
}

/** Return the value of an item */
static const void *fhdr_item_value(const struct fhdr_tag_length *item)
{
	return (const void *)((uintptr_t)item + sizeof(*item));
}

const void *fhdr_lookup_tag(const void *blob, enum fhdr_tag tag, uint32_t *length)
{
	const struct fhdr_tag_length *fhdr = (const struct fhdr_tag_length *)((char *)blob + 8);
	const struct fhdr_tag_length *item = fhdr;
	uint32_t fhdr_length;

	SCSC_TAG_DBG4(FW_LOAD, "blob: %p fhdr->tag: %u\n", blob, fhdr->tag);

	if (fhdr->tag != FHDR_TAG_TOTAL_LENGTH) {
		SCSC_TAG_WARNING(FW_LOAD, "Unexpected first tag in fhdr: %u \n", item->tag);
		return NULL;
	}

	/* First item is BHCD_TAG_TOTAL_LENGTH which contains overall length of the BHCD */
	fhdr_length = *(const uint32_t *)fhdr_item_value(fhdr);

	SCSC_TAG_DBG4(FW_LOAD, "fhdr_lookup_tag %u %u\n", tag, fhdr_length);

	while (item != NULL) {
		SCSC_TAG_DBG4(FW_LOAD, "blob: %p item->tag: %u item->length: %u\n", blob, item->tag, item->length);
		if (item->tag == tag) {
			if (length != NULL) {
				*length = item->length;
			}
			SCSC_TAG_DBG4(FW_LOAD, "fhdr_lookup_tag %u found with size %u\n", item->tag, item->length);
			return fhdr_item_value(item);
		}
		item = fhdr_next_item(item, &fhdr_length);
	}

	return NULL;
}
