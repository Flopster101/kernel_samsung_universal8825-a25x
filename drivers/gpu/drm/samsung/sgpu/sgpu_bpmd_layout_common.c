/*
 * @file sgpu_bpmd_layout_common.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * @brief implement bpmd common layout functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include "amdgpu.h"
#include "amdgpu_ring.h"
#include "sgpu_bpmd.h"
#include "sgpu_bpmd_layout_common.h"

/**
 * sgpu_bpmd_layout_dump_header_version - dump a bpmd header with a version
 *
 * @f: pointer to a file
 * @major: major version
 * @minor: minor version
 *
 */
void sgpu_bpmd_layout_dump_header_version(struct file *f,
					  uint32_t major,
					  uint32_t minor)
{
	ssize_t r = 0;
	struct sgpu_bpmd_layout_header header = {{0}};

	header.magic[0] = SGPU_BPMD_LAYOUT_MAGIC_0;
	header.magic[1] = SGPU_BPMD_LAYOUT_MAGIC_1;
	header.magic[2] = SGPU_BPMD_LAYOUT_MAGIC_2;
	header.magic[3] = SGPU_BPMD_LAYOUT_MAGIC_3;
	header.major = major;
	header.minor = minor;

	r =  sgpu_bpmd_write(f, (const void*) &header, sizeof(header),
			     SGPU_BPMD_LAYOUT_PACKET_ALIGN_BYTE);
	if (r < 0) {
		DRM_ERROR("failed to dump bpmd header %ld", r);
	}
}

/**
 * sgpu_bpmd_layout_get_ring_type - convert to a bpmd ring type
 *
 * @type: amdgpu ring type
 *
 * Return a bpmd ring type
 */
enum sgpu_bpmd_layout_ring_type
sgpu_bpmd_layout_get_ring_type(enum amdgpu_ring_type type)
{
	enum sgpu_bpmd_layout_ring_type r = 0;
	switch(type) {
	case AMDGPU_RING_TYPE_GFX:
		r = SGPU_BPMD_LAYOUT_RING_TYPE_GFX;
		break;
	case AMDGPU_RING_TYPE_COMPUTE:
		r = SGPU_BPMD_LAYOUT_RING_TYPE_COMPUTE;
		break;
	default:
		DRM_ERROR("Invalid ring type");
		BUG();
		break;
	}
	return r;
}
