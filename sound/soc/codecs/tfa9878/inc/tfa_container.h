/*
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright 2020 GOODIX, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFACONTAINER_H_
#define TFACONTAINER_H_

/* static limits */
#define TFACONT_MAXDEVS (4) /* maximum nr of devices */
#define TFACONT_MAXPROFS (64) /* maximum nr of profiles */

#include "tfa98xx_parameters.h"

/*
 * Pass the container buffer, initialize and allocate internal memory.
 *
 * @param cnt pointer to the start of the buffer holding the container file
 * @param length of the data in bytes
 * @return
 *  - tfa_error_ok if normal
 *  - tfa_error_container invalid container data
 *  - tfa_error_bad_param invalid parameter
 *
 */
enum tfa_error tfa_load_cnt(void *cnt, int length);

/*
 * Return the descriptor string
 * @param cnt pointer to the container struct
 * @param dsc pointer to Tfa descriptor
 * @return descriptor string
 */
char *tfa_cont_get_string(struct tfa_container *cnt,
	struct tfa_desc_ptr *dsc);

/*
 * Gets the string for the given command type number
 * @param type number representing a command
 * @return string of a command
 */
char *tfa_cont_get_command_string(uint32_t type);

/*
 * get the device type from the patch in this devicelist
 *  - find the patch file for this devidx
 *  - return the devid from the patch or 0 if not found
 * @param cnt pointer to container file
 * @param dev_idx device index
 * @return descriptor string
 */
int tfa_cont_get_devid(struct tfa_container *cnt, int dev_idx);

/*
 * Get the responder for the device if it exists.
 * @param tfa the device struct pointer
 * @param resp_addr the index of the device
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_get_resp(struct tfa_device *tfa,
	uint8_t *resp_addr);

void tfa_cont_set_resp(uint8_t resp_addr);

/*
 * Get the index for a responder address.
 * @param tfa the device struct pointer
 * @return the device index
 */
int tfa_cont_get_idx(struct tfa_device *tfa);

/*
 * Write reg and bitfield items in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_write_regs_dev(struct tfa_device *tfa);

/*
 * Write  reg  and bitfield items in the profilelist to the target.
 * @param tfa the device struct pointer
 * @param prof_idx the profile index
 * @return Tfa98xx_Error
 */
enum tfa98xx_error  tfa_cont_write_regs_prof(struct tfa_device *tfa,
	int prof_idx);

/*
 * Write a patchfile in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_write_patch(struct tfa_device *tfa);

/*
 * Write all  param files in the devicelist to the target.
 * @param tfa the device struct pointer
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_write_files(struct tfa_device *tfa);

/*
 * Get sample rate from passed profile index
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @return sample rate value
 */
unsigned int tfa98xx_get_profile_sr(struct tfa_device *tfa,
	unsigned int prof_idx);

/*
 * Get value for the given bitfield, defined in container file
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return bitfield value (Max2)
 */
unsigned int tfa98xx_get_cnt_bitfield(struct tfa_device *tfa,
	uint16_t bitfield);

/*
 * Get the device name string
 * @param cnt the pointer to the container struct
 * @param dev_idx the index of the device
 * @return device name string or error string if not found
 */
char *tfa_cont_device_name(struct tfa_container *cnt, int dev_idx);

/*
 * Get the application name from the container file application field
 * @param tfa the device struct pointer
 * @param name the input stringbuffer with size: sizeof(application field)+1
 * @return actual string length
 */
int tfa_cont_get_app_name(struct tfa_device *tfa, char *name);

/*
 * Get profile index of the calibration profile
 * @param tfa the device struct pointer
 * @return profile index, -2 if no calibration profile is found or -1 on error
 */
int tfa_cont_get_cal_profile(struct tfa_device *tfa);

/*
 * Is the profile a tap profile ?
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @return 1 if the profile is a tap profile or 0 if not
 */
int tfa_cont_is_tap_profile(struct tfa_device *tfa, int prof_idx);

/*
 * Is the profile specific to device ?
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return 1 if the profile belongs to device or 0 if not
 */
int tfa_cont_is_dev_specific_profile(struct tfa_container *cnt,
	int dev_idx, int prof_idx);

/*
 * Get the name of profile with index for a device in container file
 * @param cnt the pointer to the container struct
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return profile name string or error string if not found
 */
char *tfa_cont_profile_name(struct tfa_container *cnt,
	int dev_idx, int prof_idx);

/*
 * Process all items in the profilelist
 * NOTE an error return during processing will leave the device muted
 * @param tfa the device struct pointer
 * @param prof_idx index of the profile
 * @param vstep_idx index of the vstep
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_write_profile(struct tfa_device *tfa,
	int prof_idx, int vstep_idx);

/*
 * Specify the speaker configurations (cmd id) (Left, right, both, none)
 * @param dev_idx index of the device
 * @param configuration name string of the configuration
 */
void tfa98xx_set_spkr_select(int dev_idx, char *configuration);

enum tfa98xx_error tfa_cont_write_filterbank(struct tfa_device *tfa,
	struct tfa_filter *filter);

/*
 * Write all  param files in the profilelist to the target
 * this is used during startup when maybe ACS is set
 * @param tfa the device struct pointer
 * @param prof_idx the index of the profile
 * @param vstep_idx the index of the vstep
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_write_files_prof(struct tfa_device *tfa,
	int prof_idx, int vstep_idx);
enum tfa98xx_error tfa_cont_write_files_vstep(struct tfa_device *tfa,
	int prof_idx, int vstep_idx);
enum tfa98xx_error tfa_cont_write_drc_file(struct tfa_device *tfa,
	int size, uint8_t data[]);

/*
 * Get the device list dsc from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @return device list pointer
 */
struct tfa_device_list *tfa_cont_get_dev_list(struct tfa_container *cont,
	int dev_idx);

/*
 * Get the Nth profile for the Nth device
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @param prof_idx the index of the profile
 * @return profile list pointer
 */
struct tfa_profile_list *tfa_cont_get_dev_prof_list
	(struct tfa_container *cont, int dev_idx, int prof_idx);

/*
 * Get the number of profiles for device from contaienr
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @return device list pointer
 */
int tfa_cnt_get_dev_nprof(struct tfa_device *tfa);

/*
 * Get the Nth livedata for the Nth device
 * @param cont pointer to the tfaContainer
 * @param dev_idx the index of the device
 * @param livedata_idx the index of the livedata
 * @return livedata list pointer
 */
struct tfa_livedata_list *tfa_cont_get_dev_livedata_list
	(struct tfa_container *cont, int dev_idx, int livedata_idx);

/*
 * Check CRC for container
 * @param cont pointer to the tfaContainer
 * @return error value 0 on error
 */
int tfa_cont_crc_check_container(struct tfa_container *cont);

/*
 * Get the device list pointer
 * @param cnt pointer to the container struct
 * @param dev_idx the index of the device
 * @return pointer to device list
 */
struct tfa_device_list *tfa_cont_device(struct tfa_container *cnt,
	int dev_idx);

/*
 * Return the pointer to the first profile in a list from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @return pointer to first profile in profile list
 */
struct tfa_profile_list *tfa_cont_get_1st_prof_list(struct tfa_container *cont);

/*
 * Return the pointer to the next profile in a list
 * @param prof is the pointer to the profile list
 * @return profile list pointer
 */
struct tfa_profile_list *tfa_cont_next_profile(struct tfa_profile_list *prof);

/*
 * Return the pointer to the first livedata in a list from the tfaContainer
 * @param cont pointer to the tfaContainer
 * @return pointer to first livedata in profile list
 */
struct tfa_livedata_list *tfa_cont_get_1st_livedata_list
	(struct tfa_container *cont);

/*
 * Return the pointer to the next livedata in a list
 * @param livedata_idx is the pointer to the livedata list
 * @return livedata list pointer
 */
struct tfa_livedata_list *tfa_cont_next_livedata
	(struct tfa_livedata_list *livedata_idx);

/*
 * Write a bit field
 * @param tfa the device struct pointer
 * @param bf bitfield to write
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_run_write_bitfield(struct tfa_device *tfa,
	struct tfa_bitfield bf);

/*
 * Check FW API between the algorithm and the msg / volstep file
 * @param tfa the device struct pointer
 * @param hdrstr customer string from header
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_fw_api_check(struct tfa_device *tfa,
	char *hdrstr);

/*
 * Write a parameter file to the device
 * @param tfa the device struct pointer
 * @param file filedescriptor pointer
 * @param vstep_idx index to vstep
 * @param vstep_msg_idx index to vstep message
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_cont_write_file(struct tfa_device *tfa,
	struct tfa_file_dsc *file, int vstep_idx, int vstep_msg_idx);

/*
 * Get the max volume step associated with Nth profile for the Nth device
 * @param tfa the device struct pointer
 * @param prof_idx profile index
 * @return the number of vsteps
 */
int tfa_cont_get_max_vstep(struct tfa_device *tfa, int prof_idx);

/*
 * Get the file contents associated with the device or profile
 * Search within the device tree, if not found, search within the profile
 * tree. There can only be one type of file within profile or device.
 * @param tfa the device struct pointer
 * @param prof_idx I2C profile index in the device
 * @param type file type
 * @return 0 NULL if file type is not found
 * @return 1 file contents
 */
struct tfa_file_dsc *tfa_cont_get_file_data(struct tfa_device *tfa,
	int prof_idx, enum tfa_header_type type);

/*
 * Dump the contents of the file header
 * @param hdr pointer to file header data
 */
void tfa_cont_show_header(struct tfa_header *hdr);

/*
 * Read a bit field
 * @param tfa the device struct pointer
 * @param bf bitfield to read out
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_run_read_bitfield(struct tfa_device *tfa,
	struct tfa_bitfield *bf);

/*
 * Get hw feature bits from container file
 * @param tfa the device struct pointer
 * @param hw_feature_register pointer to where hw features are stored
 */
void get_hw_features_from_cnt(struct tfa_device *tfa,
	int *hw_feature_register);

/*
 * Get sw feature bits from container file
 * @param tfa the device struct pointer
 * @param sw_feature_register pointer to where sw features are stored
 */
void get_sw_features_from_cnt(struct tfa_device *tfa,
	int sw_feature_register[2]);

/*
 * Factory trimming for the Boost converter
 * check if there is a correction needed
 * @param tfa the device struct pointer
 */
enum tfa98xx_error tfa98xx_factory_trimmer(struct tfa_device *tfa);

/*
 * Search for filters settings and if found then write them to the device
 * @param tfa the device struct pointer
 * @param prof_idx profile to look in
 * @return Tfa98xx_Error
 */
enum tfa98xx_error tfa_set_filters(struct tfa_device *tfa, int prof_idx);

/*
 * Get the firmware version from the patch in the container file
 * @param tfa the device struct pointer
 * @return firmware version
 */
int tfa_cnt_get_patch_version(struct tfa_device *tfa);

enum tfa_blob_index {
	BLOB_INDEX_REGULAR,
	BLOB_INDEX_INDIVIDUAL,
	BLOB_INDEX_MAX
};

int tfa_tib_dsp_msgmulti(struct tfa_device *tfa, int length,
	const char *buffer);

#endif /* TFACONTAINER_H_ */
