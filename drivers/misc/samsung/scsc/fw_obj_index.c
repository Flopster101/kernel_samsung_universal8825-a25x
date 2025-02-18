#include "fw_obj_index.h"

void *fw_obj_index_lookup_tag(const void *blob, u8 tag_id, uint32_t *size)
{
	struct object_index_entry *object_index_entry;
	struct object_header *object_header;
	uintptr_t object_offset;
	uint32_t index_count;
	uint32_t *header_offset;
	uint32_t i;

	object_index_entry = (struct object_index_entry *)(blob + HEADER_OFFSET_OBJECT_INDEX);
	/* Get object index metadata */
	object_offset = (uintptr_t)object_index_entry->object_offset;
	index_count = *(uint32_t *)(object_index_entry + 1) / 4;
	SCSC_TAG_INFO(MX_FW, "Object index offset : %d count %d\n", object_offset, index_count);

	/* Traverse to find the tag_id and return the memory pointer  */
	for (i = 0; i < index_count; i++) {
		header_offset = (uint32_t *)(blob + object_offset + i * 4);
		object_header = (struct object_header *)(blob + *header_offset);

		SCSC_TAG_INFO(MX_FW, "offset : 0x%x, ID : 0x%x, size : 0x%x\n",
			*header_offset,
			object_header->object_id,
			object_header->object_size - sizeof(struct object_header));

		if (object_header->object_id == tag_id) {
			*size = (uint32_t)(object_header->object_size - sizeof(struct object_header));
			SCSC_TAG_INFO(MX_FW, "tag_id 0x%x found 0x%d\n", tag_id, size);
			return ((void *)object_header + sizeof(object_header));
		}

	}

	SCSC_TAG_INFO(MX_FW, "tag_id 0x%x not found\n", tag_id);
	return NULL;
}
