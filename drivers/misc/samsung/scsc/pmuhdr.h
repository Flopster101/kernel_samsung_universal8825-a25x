/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/*
 * PMU Firmware File Header (PMUHDR)
 */

#ifndef __PMUHDR_H__
#define __PMUHDR_H__

struct pmu_firmware_header {
	uint32_t hdr_version; /* header version*/
	uint32_t hdr_size; /* total size of header */
	uint32_t fw_offset; /* offset to pmu code (from header start) */
	uint32_t fw_size; /* size of pmu code */
	uint32_t fw_patch_ver; /* pmu code version */
};

#endif /* __PMUHDR_H__ */
