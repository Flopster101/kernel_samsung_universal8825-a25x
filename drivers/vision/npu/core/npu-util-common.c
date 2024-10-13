/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/ktime.h>
#include <linux/slab.h>

#include "npu-util-common.h"
#include "npu-log.h"

/*
 * Time calculation
 */
inline s64 npu_get_time_ns(void)
{
	return ktime_to_ns(ktime_get_boottime());
}

inline s64 npu_get_time_us(void)
{
	return ktime_to_us(ktime_get_boottime());
}

void npu_util_bitmap_dump(struct npu_util_bitmap *map)
{
	npu_info("bitmap[%s] dump : size(%u)/used_size(%u)/base_bit(%u)\n",
			map->name, map->bitmap_size, map->used_size,
			map->base_bit);
	print_hex_dump(KERN_NOTICE, "[Exynos][NPU][NOTICE]: bitmap raw: ",
			DUMP_PREFIX_NONE, 32, 8, map->bitmap,
			BITS_TO_LONGS(map->bitmap_size) * sizeof(long), false);
}

int npu_util_bitmap_set_region(struct npu_util_bitmap *map, unsigned int size)
{
	int ret;
	unsigned long start, end, check;
	bool turn = false;

	if (!size) {
		ret = -EINVAL;
		npu_err("Invalid bitmap size[%s](%u)\n", map->name, size);
		goto p_err;
	}

	if (size > map->bitmap_size - map->used_size) {
		ret = -ENOMEM;
		npu_err("Not enough bitmap[%s](%u)\n", map->name, size);
		goto p_err;
	}

	start = map->base_bit;
again:
	start = find_next_zero_bit(map->bitmap, map->bitmap_size, start);

	end = start + size - 1;
	if (end >= map->bitmap_size) {
		if (turn) {
			ret = -ENOMEM;
			npu_err("Not enough contiguous bitmap[%s](%u)\n",
					map->name, size);
			goto p_err;
		} else {
			turn = true;
			start = 0;
			goto again;
		}
	}

	check = find_next_bit(map->bitmap, end, start);
	if (check < end) {
		start = check + 1;
		goto again;
	}

	bitmap_set(map->bitmap, start, size);
	map->base_bit = end + 1;
	map->used_size += size;

	return start;
p_err:
	npu_util_bitmap_dump(map);
	return ret;
}

void npu_util_bitmap_clear_region(struct npu_util_bitmap *map,
		unsigned int start, unsigned int size)
{
	/* In the case of a unified op model,
	 * several sessions may have the same unified id.
	 */
	if (!map->used_size) {
		npu_dbg("already bitmap[%s] clear (%u/%u)\n", map->name, start, size);
		return;
	}

	if ((map->bitmap_size < start + size - 1) ||
			size > map->used_size) {
		npu_warn("Invalid clear parameter[%s](%u/%u)\n",
				map->name, start, size);
		npu_util_bitmap_dump(map);
		return;
	}

	map->used_size -= size;
	bitmap_clear(map->bitmap, start, size);
}

int npu_util_bitmap_init(struct npu_util_bitmap *map, const char *name,
		unsigned int size)
{
	int ret = 0;

	if (!size) {
		ret = -EINVAL;
		npu_err("bitmap size can not be zero\n");
		goto p_err;
	}

	map->bitmap = kzalloc(BITS_TO_LONGS(size) * sizeof(long), GFP_KERNEL);
	if (!map->bitmap) {
		ret = -ENOMEM;
		npu_err("Failed to init bitmap(%u/%lu)\n",
				size, BITS_TO_LONGS(size) * sizeof(long));
		goto p_err;
	}

	snprintf(map->name, NPU_BITMAP_NAME_LEN, "%s", name);
	map->bitmap_size = size;
	map->used_size = 0;
	map->base_bit = 0;

p_err:
	return ret;
}

void npu_util_bitmap_zero(struct npu_util_bitmap *map)
{
	map->used_size = 0;
	map->base_bit = 0;
	bitmap_zero(map->bitmap, map->bitmap_size);
}

void npu_util_bitmap_deinit(struct npu_util_bitmap *map)
{
	kfree(map->bitmap);
}

/*
 * Validate memory range of offsets in NCP. Should not extend beyond NCP size.
 * Return error if check fails.
 */
static inline int validate_ncp_offset_range(u32 offset, u32 size, u32 cnt, size_t ncp_size)
{
	size_t start = offset, end = start + size * cnt;

	if (!start || !cnt || !size) /* Redundant entry. Ignore. */ \
		return 0;

	if (end > ncp_size)
		return -EFAULT;

	return 0;
}

static int validate_ncp_header(struct ncp_header *ncp_header, size_t ncp_size)
{
	/* Validate NCP version. */
	if (ncp_header->hdr_version != NCP_VERSION) {
		npu_err("NCP version mismatch. user=0x%x, expected=0x%x\n",
			ncp_header->hdr_version, NCP_VERSION);
		return -EFAULT;
	}

	/* Validate vector memory range. */

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->address_vector_offset,
		sizeof(struct address_vector), ncp_header->address_vector_cnt, ncp_size),
		"address vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->address_vector_offset, ncp_header->address_vector_cnt, ncp_size);

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->memory_vector_offset,
		sizeof(struct memory_vector), ncp_header->memory_vector_cnt, ncp_size),
		"memory vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->memory_vector_offset, ncp_header->memory_vector_cnt, ncp_size);

#if NCP_VERSION >= 25
	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->pwrest_vector_offset,
		sizeof(struct pwr_est_vector), ncp_header->pwrest_vector_cnt, ncp_size),
		"power estimation vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->pwrest_vector_offset, ncp_header->pwrest_vector_cnt, ncp_size);

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->interruption_vector_offset,
		sizeof(struct interruption_vector), ncp_header->interruption_vector_cnt, ncp_size),
		"interruption vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->interruption_vector_offset, ncp_header->interruption_vector_cnt,
		ncp_size);

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->llc_vector_offset,
	sizeof(struct llc_vector), ncp_header->llc_vector_cnt, ncp_size),
	"llc vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
	ncp_header->llc_vector_offset, ncp_header->llc_vector_cnt, ncp_size);
#endif

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->group_vector_offset,
		sizeof(struct group_vector), ncp_header->group_vector_cnt, ncp_size),
		"group vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->group_vector_offset, ncp_header->group_vector_cnt, ncp_size);

	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->thread_vector_offset,
		sizeof(struct thread_vector), ncp_header->thread_vector_cnt, ncp_size),
		"thread vector out of bounds. start=0x%x, cnt=%u, ncp_size=0x%lx\n",
		ncp_header->thread_vector_offset, ncp_header->thread_vector_cnt, ncp_size);

	/* Validate body memory range. */
	NPU_ERR_RET(validate_ncp_offset_range(ncp_header->body_offset,
		ncp_header->body_size, 1, ncp_size),
		"body out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
		ncp_header->body_offset, ncp_header->body_size, ncp_size);

	return 0;
}

static int validate_ncp_memory_vector(struct ncp_header *ncp_header, size_t ncp_size)
{
	u32 mv_cnt, mv_offset, av_index, av_offset;
	struct memory_vector *mv;
	struct address_vector *av;
	int i;

	mv_cnt = ncp_header->memory_vector_cnt;
	mv_offset = ncp_header->memory_vector_offset;
	av_offset = ncp_header->address_vector_offset;

	mv = (struct memory_vector *)((void *)ncp_header + mv_offset);
	av = (struct address_vector *)((void *)ncp_header + av_offset);

	for (i = 0; i < mv_cnt; i++) {

		/* Validate address vector index */
		av_index = mv[i].address_vector_index;
		if (av_index >= ncp_header->address_vector_cnt) {
			npu_err("memory vector %d has invalid address vector index %d\n",
				i, av_index);
			return -EFAULT;
		}

		/* Validate address vector m_addr */
		switch (mv[i].type) {
		case MEMORY_TYPE_CUCODE:
			fallthrough;
		case MEMORY_TYPE_WEIGHT:
			fallthrough;
		case MEMORY_TYPE_WMASK:
			NPU_ERR_RET(validate_ncp_offset_range(av[av_index].m_addr,
				av[av_index].size, 1, ncp_size),
				"m_addr out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
				av[av_index].m_addr, av[av_index].size, ncp_size);
			break;
		default:
			break;
		}
	}

	return 0;
}

#if NCP_VERSION >= 25
static int validate_ncp_interruption_vector(struct ncp_header *ncp_header, size_t ncp_size)
{
	u32 int_v_cnt, int_v_offset;
	struct interruption_vector *int_v;
	int i;

	int_v_cnt = ncp_header->interruption_vector_cnt;
	int_v_offset = ncp_header->interruption_vector_offset;
	int_v = (struct interruption_vector *)((void *)ncp_header + int_v_offset);

	for (i = 0; i < int_v_cnt; i++) {

		/* Validate isa out of bounds. */
		NPU_ERR_RET(validate_ncp_offset_range(
			ncp_header->body_offset + int_v[i].isa_offset,
			int_v[i].isa_size, 1, ncp_size),
			"interruption vector isa out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
			ncp_header->body_offset + int_v[i].isa_offset, int_v[i].isa_size, ncp_size);
	}

	return 0;
}
#endif

static int validate_ncp_group_vector(struct ncp_header *ncp_header, size_t ncp_size)
{
	u32 gv_cnt, gv_offset;
	struct group_vector *gv;
	int i;

	gv_cnt = ncp_header->group_vector_cnt;
	gv_offset = ncp_header->group_vector_offset;
	gv = (struct group_vector *)((void *)ncp_header + gv_offset);

	for (i = 0; i < gv_cnt; i++) {

		/* Validate isa memory out of bounds. */
		NPU_ERR_RET(validate_ncp_offset_range(
			ncp_header->body_offset + gv[i].isa_offset, gv[i].isa_size, 1, ncp_size),
			"group vector isa out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
			ncp_header->body_offset + gv[i].isa_offset, gv[i].isa_size, ncp_size);

		/* Validate intrinsic memory out of bounds. */
		NPU_ERR_RET(validate_ncp_offset_range(
			ncp_header->body_offset + gv[i].intrinsic_offset,
			gv[i].intrinsic_size, 1, ncp_size),
			"group vector intrinsic out of bounds. start=0x%x, size=0x%x, ncp_size=0x%lx\n",
			ncp_header->body_offset + gv[i].intrinsic_offset,
			gv[i].intrinsic_size, ncp_size);
	}

	return 0;
}

static int validate_ncp_thread_vector(struct ncp_header *ncp_header, size_t ncp_size)
{
	u32 tv_cnt, tv_offset;
	struct thread_vector *tv;
	int i;

	tv_cnt = ncp_header->thread_vector_cnt;
	tv_offset = ncp_header->thread_vector_offset;
	tv = (struct thread_vector *)((void *)ncp_header + tv_offset);

	for (i = 0; i < tv_cnt; i++) {

		/* Validate group vector index out of bounds. */
		if (ncp_header->group_vector_cnt) {
			if (tv[i].group_str_idx >= ncp_header->group_vector_cnt ||
				tv[i].group_end_idx >= ncp_header->group_vector_cnt) {
				npu_err("thread vector group index out of bounds\n");
				return -EFAULT;
			}
		}

#if NCP_VERSION >= 25
		/* Validate interruption vector index out of bounds. */
		if (ncp_header->interruption_vector_cnt) {
			if (tv[i].interruption_str_idx >= ncp_header->interruption_vector_cnt ||
				tv[i].interruption_end_idx >= ncp_header->interruption_vector_cnt) {
				npu_err("thread vector interruption index out of bounds\n");
				return -EFAULT;
			}
		}
#endif
	}

	return 0;
}

int npu_util_validate_user_ncp(struct npu_session *session, struct ncp_header *ncp_header,
				size_t ncp_size)
{
	int ret;

	ret = validate_ncp_header(ncp_header, ncp_size);
	if (ret)
		goto fail;

	ret = validate_ncp_memory_vector(ncp_header, ncp_size);
	if (ret)
		goto fail;

#if NCP_VERSION >= 25
	ret = validate_ncp_interruption_vector(ncp_header, ncp_size);
	if (ret)
		goto fail;
#endif

	ret = validate_ncp_group_vector(ncp_header, ncp_size);
	if (ret)
		goto fail;

	ret = validate_ncp_thread_vector(ncp_header, ncp_size);
	if (ret)
		goto fail;

	return 0;
fail:
	return ret;
}
