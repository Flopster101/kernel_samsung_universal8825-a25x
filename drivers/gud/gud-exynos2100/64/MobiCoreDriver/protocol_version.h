/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2020 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MC_VLX_VERSION_CONTROL_H
#define MC_VLX_VERSION_CONTROL_H

#include <linux/types.h>
#include "main.h"

#define PV_VALID_VERSION_OFFSET		(24)
#define PV_MAJOR_VERSION_OFFSET		(16)
#define PV_MINOR_VERSION_OFFSET		(8)
#define PV_PATCH_VERSION_OFFSET		(0)

#define PV_VERSION_VALID_MASK		(0xFF << 24)
#define PV_VERSION_MASK			(0x00FFFFFF)

#define PV_DRV_VERSION(MAJOR, MINOR, PATCH) \
	(((MAJOR) << PV_MAJOR_VERSION_OFFSET) | \
	((MINOR) << PV_MINOR_VERSION_OFFSET) | \
	((PATCH) << PV_PATCH_VERSION_OFFSET))

#define PV_HEADER_VERSION_V1		(0xF1E2D301)

#define PV_VERSION_VALID_CODE		(0x5A << PV_VALID_VERSION_OFFSET)

#define MC_PV_VERSION			PV_DRV_VERSION(3, 0, 0)

/*
 * Dump header, FE and BE versions.
 */
static inline void dump_hdr_versions(struct pv_driver_header_v1_t *hdr)
{
	mc_dev_info("%s Header version %x", __func__, hdr->hdr_version);
	mc_dev_info("%s FE version %x", __func__, hdr->fe_version);
	mc_dev_info("%s BE version %x", __func__, hdr->be_version);
}

/*
 * Set both header and FE versions.
 * The FE is the first one that set its versions.
 */
static inline void set_fe_driver_version(
	struct pv_driver_header_v1_t *hdr, u32 fe_version)
{
	hdr->hdr_version = PV_HEADER_VERSION_V1;
	hdr->fe_version = fe_version | PV_VERSION_VALID_CODE;

	mc_dev_devel("%s Header version %x", __func__, hdr->hdr_version);
	mc_dev_devel("%s FE version %x", __func__, hdr->fe_version);
}

/*
 * Return the FE version after checking the header version.
 * If the header is not valid, return 0.
 */
static inline u32 get_fe_driver_version(struct pv_driver_header_v1_t *hdr)
{
	if (hdr->hdr_version == PV_HEADER_VERSION_V1)
		if ((hdr->fe_version & PV_VERSION_VALID_MASK)
			== PV_VERSION_VALID_CODE)
			return hdr->fe_version & PV_VERSION_MASK;
	return 0;
}

/*
 * Set the BE version if the header is already set by the FE and valid.
 * Return false if the version is not properly set.
 */
static inline bool set_be_driver_version(
	struct pv_driver_header_v1_t *hdr, u32 be_version)
{
	if (hdr->hdr_version != PV_HEADER_VERSION_V1) {
		mc_dev_info("%s Bad header %x", __func__, hdr->hdr_version);
		return false;
	}
	hdr->be_version = be_version | PV_VERSION_VALID_CODE;
	mc_dev_devel("%s BE version %x", __func__, hdr->be_version);

	return true;
}

/*
 * Return the BE version after checking the header version.
 * If the header is not valid, return 0.
 */
static inline u32 get_be_driver_version(struct pv_driver_header_v1_t *hdr)
{
	if (hdr->hdr_version == PV_HEADER_VERSION_V1)
		if ((hdr->be_version & PV_VERSION_VALID_MASK)
			== PV_VERSION_VALID_CODE)
			return hdr->be_version & PV_VERSION_MASK;
	return 0;
}

/*
 * Compare FE and BE major versions.
 * Return true if the BE and the FE major are the same.
 */
static inline bool __is_match_major_version(u32 be_version, u32 fe_version)
{
	be_version >>= PV_MAJOR_VERSION_OFFSET;
	fe_version >>= PV_MAJOR_VERSION_OFFSET;
	return (be_version == fe_version);
}

/*
 * Check if the FE interface is compatible.
 * First check if the BE and the FE major are the same.
 * Then check if the BE version >= base version compatibility.
 */
static inline bool __is_fe_interface_compatible(
	u32 be_version, u32 fe_version, u32 base_ver_for_inf)
{
	if (__is_match_major_version(be_version, fe_version) &&
	    be_version >= base_ver_for_inf) {
		return true;
	}
	mc_dev_info("%s FE interface not compatible", __func__);

	return false;
}

/*
 * Get the BE version compatibility for the FE command.
 */
static u32 get_cmd_be_version_compatibility(u32 cmd)
{
	u32 cmd_be_version_comp = 0xFFFFFFFF;

	switch (cmd) {
	/*
	 * Start of commands set for version 3.0.0 (Do never change for
	 * backward compat reason)
	 */
	case TEE_FE_NONE:
	case TEE_CONNECT:
	case TEE_GET_VERSION:
	/* TEE_MC_OPEN_DEVICE = 11,		SWd does not support this */
	/* TEE_MC_CLOSE_DEVICE,			SWd does not support this */
	case TEE_MC_OPEN_SESSION:
	case TEE_MC_OPEN_TRUSTLET:
	case TEE_MC_CLOSE_SESSION:
	case TEE_MC_NOTIFY:
	case TEE_MC_WAIT:
	case TEE_MC_MAP:
	case TEE_MC_UNMAP:
	case TEE_MC_GET_ERR:
	/* TEE_GP_INITIALIZE_CONTEXT = 21,	SWd does not support this */
	/* TEE_GP_FINALIZE_CONTEXT,		SWd does not support this */
	case TEE_GP_REGISTER_SHARED_MEM:
	case TEE_GP_RELEASE_SHARED_MEM:
	case TEE_GP_OPEN_SESSION:
	case TEE_GP_CLOSE_SESSION:
	case TEE_GP_INVOKE_COMMAND:
	case TEE_GP_REQUEST_CANCELLATION:
		cmd_be_version_comp = PV_DRV_VERSION(3, 0, 0);
		break;
	/*
	 * End of commands set for version 3.0.0
	 * case ANY_NEW_COMMAND: cmd_be_version_comp = PV_DRV_VERSION(x, x, x);
	 * Any new command == new version!
	 */
	default:
		cmd_be_version_comp = 0xFFFFFFFF;
		mc_dev_err(-ENOTSUPP, "%s Unkwown command %x", __func__, cmd);
		break;
	}
	return cmd_be_version_comp;
}

/*
 * Returns true if the FE interface is compatible:
 * BE and FE major are identical
 * and BE version >= command BE version compatibility
 */
static inline bool is_fe_interface_compatible(
	struct pv_driver_header_v1_t *hdr, u32 cmd)
{
	u32 be_version = 0;
	u32 fe_version = 0;
	u32 cmd_be_version_comp;

	if (hdr->hdr_version == PV_HEADER_VERSION_V1) {
		if ((hdr->be_version & PV_VERSION_VALID_MASK)
			!= PV_VERSION_VALID_CODE) {
			mc_dev_err(-EINVAL, "%s Bad BE header version %x",
				   __func__, hdr->hdr_version);
			return false;
		}
		be_version = hdr->be_version & PV_VERSION_MASK;

		if ((hdr->fe_version & PV_VERSION_VALID_MASK)
			!= PV_VERSION_VALID_CODE) {
			mc_dev_err(-EINVAL, "%s Bad FE header version %x",
				   __func__, hdr->hdr_version);
			return false;
		}
		fe_version = hdr->fe_version & PV_VERSION_MASK;
	}
	be_version = hdr->be_version & PV_VERSION_MASK;
	fe_version = hdr->fe_version & PV_VERSION_MASK;
	cmd_be_version_comp = get_cmd_be_version_compatibility(cmd);

	mc_dev_devel("%s Header version %x", __func__, hdr->hdr_version);
	mc_dev_devel("%s FE version %x", __func__, hdr->fe_version);
	mc_dev_devel("%s BE version %x", __func__, hdr->be_version);
	mc_dev_devel("%s BE minimal version for compatibility %x",
		     __func__, cmd_be_version_comp);

	return __is_fe_interface_compatible(be_version, fe_version,
					    cmd_be_version_comp);
}

#endif /* MC_VLX_VERSION_CONTROL_H */
