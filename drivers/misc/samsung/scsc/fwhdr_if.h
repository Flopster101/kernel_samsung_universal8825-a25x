/****************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef FWHDR_IF_H
#define FWHDR_IF_H

#include "scsc_mif_abs.h"

struct fwhdr_if {
	int (*init)(struct fwhdr_if *interface, char *fw_data, size_t fw_len, bool skip_header);
	int (*do_fw_crc32_checks)(struct fwhdr_if *interface, bool crc32_over_binary);
	int (*copy_fw)(struct fwhdr_if *interface, char *fw_data, size_t fw_size, void *dram_addr);
	/* Getters */
	char *(*get_build_id)(struct fwhdr_if *interface);
	char *(*get_ttid)(struct fwhdr_if *interface);
	u32 (*get_entry_point)(struct fwhdr_if *interface);
	bool (*get_parsed_ok)(struct fwhdr_if *interface);
	bool (*get_check_crc)(struct fwhdr_if *interface);
	u32 (*get_fw_rt_len)(struct fwhdr_if *interface);
	u32 (*get_fwapi_major)(struct fwhdr_if *interface);
	u32 (*get_fwapi_minor)(struct fwhdr_if *interface);
	u32 (*get_panic_record_offset)(struct fwhdr_if *interface, enum scsc_mif_abs_target target);
	u32 (*get_fw_offset)(struct fwhdr_if *interface);
	/*CRC*/
	void (*crc_wq_stop)(struct fwhdr_if *interface);
	void (*crc_wq_start)(struct fwhdr_if *interface);
	/* Setters */
	void (*set_entry_point)(struct fwhdr_if *interface, u32 entry_point);
	void (*set_fw_rt_len)(struct fwhdr_if *interface, u32 rt_len);
	void (*set_check_crc)(struct fwhdr_if *interface, bool check_crc);
};

#endif /* FWHDR_IF_H */
