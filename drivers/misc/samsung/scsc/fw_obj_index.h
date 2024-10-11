/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <scsc/scsc_logring.h>

#ifndef FW_OBJ_INDEX_H
#define FW_OBJ_INDEX_H

/* sub fw's OBJUECT_ID */

#define PMU_8051_INIT 1
#define PMU_CM0_INIT 2
#define WLAN_FW_INIT 3
#define BT_FW_INIT 4
#define LOG_STRING_INIT 255

/* offset */

#define HEADER_OFFSET_OBJECT_INDEX 0x1A0

struct object_index_entry {
	uint32_t object_offset;
};

struct object_header {
	uint32_t object_id;
	uint32_t object_size;
};

struct pmu_header {
	uint32_t hdr_version;	/* header version*/
	uint32_t hdr_size;		/* total size of header */
	uint32_t fw_offset;		/* offset to pmu code (from header start) */
	uint32_t fw_size;		/* size of pmu code */
	uint32_t fw_patch_ver;	/* pmu code version */
};

void *fw_obj_index_lookup_tag(const void *blob, u8 tag_id, uint32_t *size);
#endif
